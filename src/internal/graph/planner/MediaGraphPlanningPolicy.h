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

    bool enableDistributedExecution = false;
    bool enableGpuPlanning = false;
    bool enableMeshPlanning = false;
    bool keepSourceAndSinkOnEdge = true;
};

} // namespace media::ffmpeg::graph
