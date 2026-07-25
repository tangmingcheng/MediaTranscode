#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlannerReport.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphDeploymentPlan.h"
#include "internal/graph/runtime/mesh/MediaMeshRoute.h"
#include "media_transcode/Result.h"

#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphMeshPlannerResult {
    std::vector<MediaMeshRoute> routes;
    MediaGraphPlannerReport report;
};

class MediaGraphMeshPlanner final {
public:
    static ::media::Result<MediaGraphMeshPlannerResult> plan(
        const MediaGraph& graph,
        const MediaGraphDeploymentPlan& deploymentPlan,
        const MediaGraphPlanningPolicy& policy = {});

};

} // namespace media::ffmpeg::graph
