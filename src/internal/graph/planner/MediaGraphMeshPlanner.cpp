#include "internal/graph/planner/MediaGraphMeshPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphMeshPlannerResult> MediaGraphMeshPlanner::plan(
    const MediaGraph& graph,
    const MediaGraphDeploymentPlan& deploymentPlan,
    const MediaGraphPlanningPolicy& policy)
{
    MediaGraphMeshPlannerResult result;

    if (!policy.enableMeshPlanning) {
        result.report.info("mesh-planner", "mesh planning disabled");
        return ::media::Result<MediaGraphMeshPlannerResult>::success(std::move(result));
    }

    return ::media::Result<MediaGraphMeshPlannerResult>::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphMeshPlanner is unsupported: distributed transport execution is not implemented"));

}

} // namespace media::ffmpeg::graph
