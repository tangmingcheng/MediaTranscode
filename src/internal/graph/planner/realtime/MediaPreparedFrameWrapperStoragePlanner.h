#pragma once
#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"

namespace media::ffmpeg::graph {
struct MediaPreparedFrameWrapperStoragePlan final {
    std::size_t sourceIndex;
    // Root evidence is owned once by the same source's lineage FrameWrapper
    // family. This numeric product retains no borrowed pointer into that plan.
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
