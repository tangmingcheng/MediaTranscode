#pragma once

#include "internal/graph/planner/MediaGraphPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"

namespace media::ffmpeg::graph {

struct MediaGraphPlannerAdapterResult {
    MediaGraphClusterTopology topology;
    MediaGraphPlanningPolicy policy;
    MediaGraphPlannerResult plannerResult;
};

} // namespace media::ffmpeg::graph
