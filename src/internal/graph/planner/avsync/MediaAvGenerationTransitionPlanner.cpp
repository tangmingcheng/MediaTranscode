#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/sync/MediaDemuxClockBinderGenerationIdentities.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

namespace media::ffmpeg::graph {
namespace {

std::vector<std::string> canonicalLineageChildren(
    MediaAvSyncSourceClockMode sourceClockMode)
{
    std::vector<std::string> children{
        "startup_generation_state",
        "video_decode",
        "video_frame_rate",
        "video_filter",
        "video_encode",
        std::string(MediaAudioDecodeLineageIdentity),
        std::string(MediaAudioStartupTrimLineageIdentity),
        std::string(MediaAudioResampleLineageIdentity),
        std::string(MediaAudioEncodeLineageIdentity),
        std::string(MediaEncodedAudioCanonicalizerLineageIdentity)
    };
    if (sourceClockMode == MediaAvSyncSourceClockMode::DemuxTimestamps) {
        children.insert(
            children.begin(),
            std::string(MediaDemuxAudioClockBinderGenerationIdentity));
        children.insert(
            children.begin(),
            std::string(MediaDemuxVideoClockBinderGenerationIdentity));
    }
    return children;
}

} // namespace

MediaAvGenerationTransitionPlan MediaAvGenerationTransitionPlanner::plan(
    MediaAvSyncOutputAdapterKind adapter,
    MediaAvSyncSourceClockMode sourceClockMode,
    MediaRunningTime acknowledgementTimeout,
    MediaRunningTime terminalDrainWindow)
{
    MediaAvGenerationTransitionPlan transition{
        {}, acknowledgementTimeout, terminalDrainWindow};
    transition.participants.push_back({
        MediaAvGenerationParticipant::CanonicalLineage,
        canonicalLineageChildren(sourceClockMode)});
    transition.participants.push_back({
        MediaAvGenerationParticipant::AudioCorrection,
        {std::string(MediaAudioCorrectionGenerationIdentity)}});
    transition.participants.push_back({
        MediaAvGenerationParticipant::Scheduler,
        {"scheduler_generation_state"}});
    if (adapter == MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        transition.participants.push_back({
            MediaAvGenerationParticipant::RtpVideoOutput,
            {"rtp_video_output_generation_state"}});
        transition.participants.push_back({
            MediaAvGenerationParticipant::RtpAudioOutput,
            {"rtp_audio_output_generation_state"}});
    } else if (adapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        transition.participants.push_back({
            MediaAvGenerationParticipant::ProjectMpegTsOutput,
            {"project_mpegts_output_generation_state",
             "scheduled_ts_adapter_generation_state",
             "project_mpegts_mux_generation_state"}});
    }
    return transition;
}

} // namespace media::ffmpeg::graph
