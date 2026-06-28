#pragma once

#include "internal/graph/model/MediaGraphOptimizationPolicy.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaZeroCopyPolicy.h"

namespace media::ffmpeg::graph {

enum class MediaGraphPlacementStrategy {
    SingleNode,
    RoundRobinWorkers,
    PreferEdgeForIo,
    PreferHardwareWorkers
};

struct MediaGraphPlanningPolicy {
    MediaGraphPlacementStrategy placementStrategy = MediaGraphPlacementStrategy::RoundRobinWorkers;
    MediaGraphOptimizationPolicy optimizationPolicy;
    MediaThreadingPolicy threadingPolicy;
    MediaZeroCopyPolicy zeroCopyPolicy;

    bool enableDistributedExecution = true;
    bool enableGpuPlanning = true;
    bool enableMeshPlanning = true;
    bool keepSourceAndSinkOnEdge = true;
};

} // namespace media::ffmpeg::graph
