#include "internal/graph/planner/realtime/MediaPreparedFrameWrapperStoragePlanner.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <new>

namespace media::ffmpeg::graph {
::media::Result<MediaPreparedFrameWrapperStoragePlan> MediaPreparedFrameWrapperStoragePlanner::plan(
    const MediaCompositionGraphTopology& topology, const MediaSourceLineageStoragePlan& lineage,
    const MediaVideoFrameProducerFacts& decoder, const std::optional<MediaVideoFrameProducerFacts>& filter)
try {
    using R = ::media::Result<MediaPreparedFrameWrapperStoragePlan>;
    const auto invalid = [](const char* message) { return R::failure(::media::ErrorInfo::invalidArgument(message)); };
    if (!mediaVideoFrameProducerMaximumExtendedReferences(decoder) ||
        !mediaVideoFrameProducerMatchesRole(decoder, MediaVideoFrameProducerRole::Decoder) ||
        (filter && (!mediaVideoFrameProducerMaximumExtendedReferences(*filter) ||
            !mediaVideoFrameProducerMatchesRole(*filter, MediaVideoFrameProducerRole::Filter))))
        return invalid("Prepared frame wrappers require admitted hardware producer contracts");
    const MediaSourceLineageAllocationCost* frames = nullptr;
    for (const auto& allocation : lineage.allocations) {
        if (allocation.family != MediaSourceLineageAllocationFamily::FrameWrapper) continue;
        if (frames) return invalid("Prepared frame wrappers have repeated lineage families");
        frames = &allocation;
    }
    if (!frames || frames->roots.empty()) return invalid("Prepared frame wrappers lack creator roots");
    MediaPreparedFrameWrapperStoragePlan result{lineage.sourceIndex, frames->roots, 0, 0};
    std::size_t decoderConstructions = 0, filterConstructions = 0;
    for (const auto& root : result.roots) {
        if (root.creatorPosition >= topology.positions.size() || !root.maximumReferences)
            return invalid("Prepared frame wrapper root is outside its topology");
        const auto& creator = topology.positions[root.creatorPosition];
        if (creator.source != lineage.sourceIndex ||
            (creator.kind != MediaNodeKind::VideoDecode && creator.kind != MediaNodeKind::VideoFilter) ||
            (root.role != MediaSourceLineageRootRole::FrameHolder && root.role != MediaSourceLineageRootRole::Construction))
            return invalid("Prepared frame wrapper root lacks an admitted creator");
        if (root.role == MediaSourceLineageRootRole::Construction) {
            if (root.maximumReferences != 1 || root.holder)
                return invalid("Prepared frame wrapper construction must be serial");
            if (creator.kind == MediaNodeKind::VideoDecode) ++decoderConstructions;
            else ++filterConstructions;
        }
        auto total = MediaCheckedArithmetic::add(result.maximumWrappers, root.maximumReferences, "prepared frame wrapper roots");
        if (!total) return R::failure(total.error());
        result.maximumWrappers = total.value();
    }
    if (decoderConstructions != 1 || filterConstructions != (filter ? 1u : 0u))
        return invalid("Prepared frame wrapper producers differ from readiness membership");
    // publish() captures hardware readiness before wrapFrame(). Capture requires a
    // non-null hw_frames_ctx, so this contract selects HardwareFrameBuffer.
    // This charges only the raw wrapper/control allocation, not AVFrame or
    // readiness/attachment storage already covered by ownership components.
    auto geometry = FFmpegBufferFactory::frameAllocationGeometry(true);
    if (!geometry) return R::failure(geometry.error());
    auto unit = MediaPreparationMemoryResource::allocationFootprint(geometry.value().bytes, geometry.value().alignment);
    if (!unit) return R::failure(unit.error());
    auto bytes = MediaCheckedArithmetic::multiply(unit.value(), result.maximumWrappers, "prepared frame wrapper bytes");
    if (!bytes) return R::failure(bytes.error());
    result.preparationBytes = bytes.value();
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedFrameWrapperStoragePlan>::failure(
        ::media::ErrorInfo::allocationFailed("prepared frame wrapper planning"));
}
}
