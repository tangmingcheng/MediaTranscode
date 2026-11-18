#include "internal/graph/planner/realtime/MediaRtpSourceProbeGoalPlanner.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRtpSourceProbeGoal> MediaRtpSourceProbeGoalPlanner::plan(
    const MediaVideoSourceTiming& timing, const std::optional<MediaRtpVideoTimingPlan>& plannedTiming,
    std::span<const MediaBufferRef> snapshot,
    const std::shared_ptr<MediaPreparationStorageBudget>& budget)
{
    using R = ::media::Result<MediaRtpSourceProbeGoal>;
    if (!plannedTiming)
        return R::failure(::media::ErrorInfo::notInitialized("Source probe requires planned video timing"));
    if (const auto* cadence = std::get_if<MediaObservedVideoCadence>(&timing)) {
        if (cadence->rate.num <= 0 || cadence->rate.den <= 0 ||
            plannedTiming->product != MediaRtpVideoTimingProduct::CanonicalInterval || plannedTiming->sourceIdentity)
            return R::failure(::media::ErrorInfo::invalidArgument("Source probe cadence is invalid"));
        return R::success(MediaFirstFrameProbeGoal{});
    }
    if (!std::holds_alternative<MediaSourcePresentationTiming>(timing) ||
        plannedTiming->product != MediaRtpVideoTimingProduct::SourcePresentation ||
        !plannedTiming->sourceIdentity || plannedTiming->sourceIdentity->empty())
        return R::failure(::media::ErrorInfo::invalidArgument("Source probe requires planned presentation identity"));
    if (snapshot.empty() || !budget)
        return R::failure(::media::ErrorInfo::invalidArgument("Source identity probe planning requires a nonempty snapshot and deployment storage"));
    const auto* first = dynamic_cast<const MediaVideoDecodeBuffer*>(snapshot.front().get());
    if (!first || first->decodedSequence() || !first->lineage() ||
        first->lineage()->identity.sourceIdentity != *plannedTiming->sourceIdentity || !first->lineage()->generation)
        return R::failure(::media::ErrorInfo::invalidArgument("Source identity probe snapshot differs from the planned source"));
    return R::success(MediaSourceIdentityProbeGoal{
        MediaSourceIdentityProbeRequirement::MappedFrame,
        first->lineage()->identity.sourceIdentity, first->lineage()->generation, budget});
}

} // namespace media::ffmpeg::graph
