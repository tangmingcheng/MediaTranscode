#pragma once
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"

namespace media::ffmpeg::graph {
// A missing closing record is benign only when the exact old generation is
// demonstrably retiring. Poisoned or unrelated transitions remain errors.
inline bool mediaAggregateSourceDrainRetired(const MediaAvSyncGroupRuntime& group,
    const MediaOwnerThreadGenerationPurge& owner, std::uint64_t generation)
{
    const auto epoch = group.epochTransitionSnapshot();
    const auto reacquisition = group.reacquisitionSnapshot();
    if (epoch.poisoned || epoch.outputPermitted) return false;
    const auto pending = owner.pending();
    if (pending && pending->oldGeneration == generation && pending->nextGeneration > generation)
        return true;
    return reacquisition.phase == MediaAvReacquisitionPhase::Purging &&
        reacquisition.transition && reacquisition.transition->oldGeneration == generation &&
        reacquisition.transition->nextGeneration > generation;
}
struct MediaAggregateSourceDrainBoundary final {
    std::uint64_t generation;
    enum class Disposition { Closing, Retired } disposition;
};
inline ::media::Result<std::optional<MediaAggregateSourceDrainBoundary>> mediaAggregateSourceDrain(
    const MediaBufferRef& buffer, const MediaAvSyncGroupRuntime& group,
    const MediaOwnerThreadGenerationPurge& owner, MediaStreamKind member)
{
    using Result = ::media::Result<std::optional<MediaAggregateSourceDrainBoundary>>;
    auto generation = mediaEndGeneration(buffer);
    if (!generation) return Result::failure(generation.error());
    if (!generation.value()) return Result::success(std::nullopt);
    const auto closing = group.sourceDrainSnapshot();
    const auto epoch = group.epochTransitionSnapshot();
    if (closing && closing->request.playbackGeneration != *generation.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate generation end changed source identity"));
    if (!closing || !epoch.outputPermitted) {
        if (mediaAggregateSourceDrainRetired(group, owner, *generation.value()))
            return Result::success(MediaAggregateSourceDrainBoundary{*generation.value(),
                MediaAggregateSourceDrainBoundary::Disposition::Retired});
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate generation end lacks exact source closing authority"));
    }
    if (epoch.poisoned || !epoch.playbackEpoch() || epoch.playbackEpoch()->generation != *generation.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate generation end has an incompatible playback epoch"));
    const auto* progress = member == MediaStreamKind::Video ? &closing->video :
        member == MediaStreamKind::Audio && closing->audio ? &*closing->audio : nullptr;
    if (!progress || !progress->acceptedFrontier)
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate generation end precedes member ingress sealing"));
    return Result::success(MediaAggregateSourceDrainBoundary{*generation.value(),
        MediaAggregateSourceDrainBoundary::Disposition::Closing});
}
} // namespace media::ffmpeg::graph
