#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/sync/MediaDemuxClockBinderGenerationIdentities.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

enum class ProcessingDomain { SharedSourceOutput, SourceContribution };

std::vector<MediaAvGenerationParticipantPlan> processingParticipants(
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaBranchMode audioBranchMode,
    bool videoFilterActive,
    ProcessingDomain domain)
{
    std::vector<std::string> children{
        "startup_generation_state",
        "video_decode",
        "video_frame_rate"
    };
    if (videoFilterActive) children.push_back("video_filter");
    if (domain == ProcessingDomain::SharedSourceOutput) children.push_back("video_encode");
    if (audioBranchMode == MediaBranchMode::TranscodeFrame) {
        children.insert(children.end(), {
            std::string(MediaAudioDecodeLineageIdentity),
            std::string(MediaAudioStartupTrimLineageIdentity),
            std::string(MediaAudioResampleLineageIdentity)});
        if (domain == ProcessingDomain::SharedSourceOutput) {
            children.insert(children.end(), {
                std::string(MediaAudioEncodeLineageIdentity),
                std::string(MediaEncodedAudioCanonicalizerLineageIdentity)});
        }
    }
    if (sourceClockMode == MediaAvSyncSourceClockMode::DemuxTimestamps) {
        children.insert(
            children.begin(),
            std::string(MediaDemuxAudioClockBinderGenerationIdentity));
        children.insert(
            children.begin(),
            std::string(MediaDemuxVideoClockBinderGenerationIdentity));
    }
    if (domain == ProcessingDomain::SourceContribution) children.push_back("aggregate_source");
    std::vector<MediaAvGenerationParticipantPlan> participants;
    participants.push_back({MediaAvGenerationParticipant::CanonicalLineage, std::move(children)});
    if (audioBranchMode == MediaBranchMode::TranscodeFrame) {
        participants.push_back({MediaAvGenerationParticipant::AudioCorrection,
            {std::string(MediaAudioCorrectionGenerationIdentity)}});
    }
    return participants;
}

} // namespace

::media::Result<MediaAvGenerationTransitionPlan>
MediaAvGenerationTransitionPlanner::planSourceContribution(
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaBranchMode audioBranchMode,
    bool videoFilterActive,
    MediaRunningTime acknowledgementTimeout,
    MediaRunningTime terminalDrainWindow)
{
    if ((sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports &&
         sourceClockMode != MediaAvSyncSourceClockMode::MpegTsPcr &&
         sourceClockMode != MediaAvSyncSourceClockMode::DemuxTimestamps) ||
        audioBranchMode != MediaBranchMode::TranscodeFrame ||
        acknowledgementTimeout <= MediaRunningTime::fromNanoseconds(0) ||
        terminalDrainWindow <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaAvGenerationTransitionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "source contribution transition requires an explicit clock, frame audio and positive timing"));
    }
    return ::media::Result<MediaAvGenerationTransitionPlan>::success({
        processingParticipants(sourceClockMode, audioBranchMode, videoFilterActive,
            ProcessingDomain::SourceContribution),
        acknowledgementTimeout, terminalDrainWindow});
}

MediaAvGenerationTransitionPlan MediaAvGenerationTransitionPlanner::plan(
    const std::variant<MediaSeparateRtpOutputRuntimePlan, MediaProjectMpegTsRuntimeOutputPlan>& output,
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaBranchMode audioBranchMode,
    bool videoFilterActive,
    MediaRunningTime acknowledgementTimeout,
    MediaRunningTime terminalDrainWindow)
{
    MediaAvGenerationTransitionPlan transition{
        processingParticipants(sourceClockMode, audioBranchMode, videoFilterActive,
            ProcessingDomain::SharedSourceOutput), acknowledgementTimeout, terminalDrainWindow};
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
