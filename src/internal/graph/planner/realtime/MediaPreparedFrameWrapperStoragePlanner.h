#pragma once
#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"

namespace media::ffmpeg::graph {
struct MediaPreparedFrameWrapperStoragePlan final {
    std::size_t sourceIndex;
    std::vector<MediaSourceLineageAllocationRoot> roots;
    std::uint64_t maximumWrappers;
    std::uint64_t preparationBytes;
};
class MediaPreparedFrameWrapperStoragePlanner final {
public:
    static ::media::Result<MediaPreparedFrameWrapperStoragePlan> plan(
        const MediaCompositionGraphTopology& topology, const MediaSourceLineageStoragePlan& lineage,
        const MediaVideoFrameProducerFacts& decoder,
        const std::optional<MediaVideoFrameProducerFacts>& filter);
};
}
