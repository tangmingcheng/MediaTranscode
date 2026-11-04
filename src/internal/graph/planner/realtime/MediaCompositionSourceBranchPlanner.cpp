#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"

namespace media::ffmpeg::graph {
::media::Result<MediaCompositionSourceBranchPlan> MediaCompositionSourceBranchPlanner::plan(
    const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
    const MediaRealtimeEdgePolicySet& policies)
{
    using R = ::media::Result<MediaCompositionSourceBranchPlan>;
    const bool canonical = std::holds_alternative<MediaObservedVideoCadence>(timing);
    const MediaVideoBranchConnectionPolicies videoPolicies{policies.metadata, policies.startupVideoRelease,
        policies.synchronizedVideoFrame, policies.synchronizedVideoFrame, policies.preparedVideoFrame};
    MediaCompositionSourceBranchPlan result;
    result.videoDecoderShape = MediaVideoBranchConnectionsPlanner::sourceDecoder();
    result.videoFrameShape = MediaVideoBranchConnectionsPlanner::sourceFrames(canonical, filterActive);
    {
        auto decoder = MediaVideoBranchConnectionsPlanner::plan(
            result.videoDecoderShape, videoPolicies);
        if (!decoder) return R::failure(decoder.error());
        result.videoDecoder = std::move(decoder).value();
    }
    auto framePolicies = videoPolicies;
    if (!canonical) framePolicies.inputFrame = policies.preparedVideoFrame;
    auto frames = MediaVideoBranchConnectionsPlanner::plan(
        result.videoFrameShape, framePolicies);
    if (!frames) return R::failure(frames.error());
    result.videoFrames = std::move(frames).value();
    if (hasAudio) {
        result.audioShape = MediaAudioBranchConnectionsPlanner::source(false, true, true);
        auto audio = MediaAudioBranchConnectionsPlanner::plan(
            *result.audioShape, policies);
        if (!audio) return R::failure(audio.error());
        result.audio = std::move(audio).value();
    }
    return R::success(std::move(result));
}
} // namespace media::ffmpeg::graph
