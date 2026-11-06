#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/sync/MediaDemuxClockBinderGenerationIdentities.h"
#include "internal/graph/sync/MediaRtpIngressFrontiers.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

#include <utility>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

enum class ProcessingDomain { SharedSourceOutput, SourceContribution };

template<class Visitor> void visitProcessingChildren(
    MediaAvSyncSourceClockMode sourceClockMode, MediaBranchMode audioBranchMode,
    bool videoFilterActive, MediaVideoStartupReadiness videoReadiness,
    ProcessingDomain domain, bool selectedAudio, Visitor&& visit)
{
    if (sourceClockMode == MediaAvSyncSourceClockMode::DemuxTimestamps) {
        visit(MediaDemuxVideoClockBinderGenerationIdentity);
        visit(MediaDemuxAudioClockBinderGenerationIdentity);
    }
    visit("startup_generation_state");
    visit("video_decode");
    visit("video_preparation");
    if (videoReadiness == MediaVideoStartupReadiness::IntervalCoverage)
        visit("video_frame_rate");
    else if (videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan)
        visit("decoded_video_startup");
    if (videoFilterActive) visit("video_filter");
    if (domain == ProcessingDomain::SharedSourceOutput) visit("video_encode");
    if (audioBranchMode == MediaBranchMode::TranscodeFrame) {
        visit(MediaAudioDecodeLineageIdentity);
        visit(MediaAudioStartupTrimLineageIdentity);
        visit(MediaAudioResampleLineageIdentity);
        if (domain == ProcessingDomain::SharedSourceOutput) {
            visit(MediaAudioEncodeLineageIdentity);
            visit(MediaEncodedAudioCanonicalizerLineageIdentity);
        }
    }
    if (sourceClockMode == MediaAvSyncSourceClockMode::RtpSenderReports) {
        visit(MediaRtpVideoIngressGenerationIdentity);
        visit(MediaRtpVideoBinderGenerationIdentity);
        if (audioBranchMode != MediaBranchMode::Drop) {
            visit(MediaRtpAudioIngressGenerationIdentity);
            visit(MediaRtpAudioBinderGenerationIdentity);
        }
    }
    if (domain == ProcessingDomain::SourceContribution) {
        visit("aggregate_source");
        if (selectedAudio) visit("aggregate_audio");
    }
}

std::vector<MediaAvGenerationParticipantPlan> processingParticipants(
    MediaAvSyncSourceClockMode sourceClockMode, MediaBranchMode audioBranchMode,
    bool videoFilterActive, MediaVideoStartupReadiness videoReadiness,
    ProcessingDomain domain, bool selectedAudio)
{
    const auto visit = [&](auto&& writer) {
        visitProcessingChildren(sourceClockMode, audioBranchMode, videoFilterActive,
            videoReadiness, domain, selectedAudio, writer);
    };
    std::size_t childCount = 0;
    visit([&](std::string_view) { ++childCount; });
    std::vector<MediaAvGenerationParticipantPlan> participants;
    participants.reserve(1 + static_cast<std::size_t>(audioBranchMode == MediaBranchMode::TranscodeFrame));
    auto& canonical = participants.emplace_back();
    canonical.participant = MediaAvGenerationParticipant::CanonicalLineage;
    canonical.requiredChildren.reserve(childCount);
    visit([&](std::string_view identity) { canonical.requiredChildren.emplace_back(identity); });
    if (audioBranchMode == MediaBranchMode::TranscodeFrame) {
        auto& correction = participants.emplace_back();
        correction.participant = MediaAvGenerationParticipant::AudioCorrection;
        correction.requiredChildren.reserve(1);
        correction.requiredChildren.emplace_back(MediaAudioCorrectionGenerationIdentity);
    }
    return participants;
}

} // namespace

::media::Result<MediaAvGenerationTransitionPlan>
MediaAvGenerationTransitionPlanner::planSourceContribution(
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaBranchMode audioBranchMode,
    bool videoFilterActive,
    MediaVideoStartupReadiness videoReadiness,
    MediaRunningTime acknowledgementTimeout, bool selectedAudio)
{
    if ((videoReadiness != MediaVideoStartupReadiness::IntervalCoverage &&
         videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan) ||
        (selectedAudio && audioBranchMode != MediaBranchMode::TranscodeFrame) ||
        (sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports &&
         sourceClockMode != MediaAvSyncSourceClockMode::MpegTsPcr &&
         sourceClockMode != MediaAvSyncSourceClockMode::DemuxTimestamps) ||
        (audioBranchMode != MediaBranchMode::TranscodeFrame &&
         !(audioBranchMode == MediaBranchMode::Drop &&
           sourceClockMode == MediaAvSyncSourceClockMode::RtpSenderReports)) ||
        acknowledgementTimeout <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaAvGenerationTransitionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "source contribution transition requires an explicit clock, frame audio and positive timing"));
    }
    auto participants = processingParticipants(sourceClockMode, audioBranchMode, videoFilterActive,
        videoReadiness, ProcessingDomain::SourceContribution, selectedAudio);
    return ::media::Result<MediaAvGenerationTransitionPlan>::success({
        std::move(participants), acknowledgementTimeout});
}

MediaAvGenerationTransitionPlan MediaAvGenerationTransitionPlanner::plan(
    const std::variant<MediaSeparateRtpOutputRuntimePlan, MediaProjectMpegTsRuntimeOutputPlan>& output,
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaBranchMode audioBranchMode,
    bool videoFilterActive,
    MediaRunningTime acknowledgementTimeout)
{
    MediaAvGenerationTransitionPlan transition{
        processingParticipants(sourceClockMode, audioBranchMode, videoFilterActive,
            MediaVideoStartupReadiness::IntervalCoverage, ProcessingDomain::SharedSourceOutput, false), acknowledgementTimeout};
    transition.participants.push_back({
        MediaAvGenerationParticipant::Scheduler,
        {"scheduler_generation_state"}});
    transition.participants.push_back({
        MediaAvGenerationParticipant::DatagramTransportPlan,
        {"datagram_transport_plan_generation_state"}});
    transition.participants.push_back({
        MediaAvGenerationParticipant::DatagramSender,
        {"datagram_sender_generation_state"}});
    if (std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(output)) {
        transition.participants.push_back({
            MediaAvGenerationParticipant::RtpVideoOutput,
            {"rtp_video_output_generation_state", "rtp_video_materializer_generation_state"}});
        transition.participants.push_back({
            MediaAvGenerationParticipant::RtpAudioOutput,
            {"rtp_audio_output_generation_state", "rtp_audio_materializer_generation_state"}});
    } else {
        transition.participants.push_back({
            MediaAvGenerationParticipant::ProjectMpegTsOutput,
            {"project_mpegts_output_generation_state",
             "scheduled_ts_adapter_generation_state",
             "project_mpegts_mux_generation_state", "mpegts_materializer_generation_state"}});
    }
    if (std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(output)) {
        transition.participants.push_back({MediaAvGenerationParticipant::ProtocolDescription,
            {"rtp_sdp_generation_state"}});
    } else if (std::holds_alternative<MediaMpegTsRtpOutputPlan>(
                   std::get<MediaProjectMpegTsRuntimeOutputPlan>(output).transport)) {
        transition.participants.push_back({MediaAvGenerationParticipant::ProtocolDescription,
            {"mpegts_sdp_generation_state"}});
    }
    return transition;
}

} // namespace media::ffmpeg::graph
