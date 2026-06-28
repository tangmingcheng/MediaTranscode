#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaThreadingPolicy.h"

#include <cstddef>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRuntimeScheduleStrategy {
    GraphOrder,
    ReverseGraphOrder,
    SingleThreaded,
    PerNodeWorker,
    WorkerPool
};

struct MediaRuntimeScheduleStep {
    MediaNodeId nodeId = MediaNodeId::invalid();
    std::size_t workerIndex = 0;
    bool parallelizable = false;
};

struct MediaRuntimeSchedulePlan {
    MediaRuntimeScheduleStrategy strategy = MediaRuntimeScheduleStrategy::GraphOrder;
    MediaThreadingPolicy threadingPolicy;
    std::vector<MediaRuntimeScheduleStep> steps;

    bool empty() const noexcept
    {
        return steps.empty();
    }

    std::size_t size() const noexcept
    {
        return steps.size();
    }
};

} // namespace media::ffmpeg::graph
