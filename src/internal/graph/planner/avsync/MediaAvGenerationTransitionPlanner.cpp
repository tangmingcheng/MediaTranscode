#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"

namespace media::ffmpeg::graph {
namespace {

std::vector<std::string> canonicalLineageChildren()
{
    return {
        "startup_generation_state",
        "video_decoder_lineage_registry",
        "video_filter_lineage_registry",
        "video_encoder_lineage_registry",
        "audio_decoder_lineage_registry",
        "audio_startup_trim_lineage_registry",
        "audio_resampler_lineage_registry",
        "audio_encoder_lineage_registry"
    };
}

} // namespace

MediaAvGenerationTransitionPlan MediaAvGenerationTransitionPlanner::plan(
    MediaAvSyncOutputAdapterKind adapter,
    MediaRunningTime acknowledgementTimeout,
    MediaRunningTime terminalDrainWindow)
{
    MediaAvGenerationTransitionPlan transition{
        {}, acknowledgementTimeout, terminalDrainWindow};
    transition.participants.push_back({
        MediaAvGenerationParticipant::CanonicalLineage,
        canonicalLineageChildren()});
    transition.participants.push_back({
        MediaAvGenerationParticipant::AudioCorrection,
        {"audio_correction_generation_state"}});
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
            {"project_mpegts_output_generation_state"}});
    }
    return transition;
}

} // namespace media::ffmpeg::graph
