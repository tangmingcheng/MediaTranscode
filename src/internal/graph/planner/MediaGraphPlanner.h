#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlannerReport.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"
#include "internal/graph/runtime/distributed/MediaGraphDeploymentPlan.h"
#include "internal/graph/runtime/gpu/MediaGpuGraphCommand.h"
#include "internal/graph/runtime/mesh/MediaMeshRoute.h"
#include "media_transcode/Result.h"

#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphPlannerResult {
    MediaGraphDeploymentPlan deploymentPlan;
    MediaGpuGraphCommandList gpuCommands;
    std::vector<MediaMeshRoute> meshRoutes;
    MediaGraphPlannerReport report;
};

class MediaGraphPlanner final {
public:
    static ::media::Result<MediaGraphPlannerResult> plan(
        const MediaGraph& graph,
        const MediaGraphClusterTopology& topology,
        const MediaGraphPlanningPolicy& policy = {});
};

} // namespace media::ffmpeg::graph
