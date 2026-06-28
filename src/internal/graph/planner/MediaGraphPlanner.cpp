#include "internal/graph/planner/MediaGraphPlanner.h"

#include "internal/graph/planner/MediaGraphDeploymentPlanner.h"
#include "internal/graph/planner/MediaGraphGpuPlanner.h"
#include "internal/graph/planner/MediaGraphMeshPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphPlannerResult> MediaGraphPlanner::plan(
    const MediaGraph& graph,
    const MediaGraphClusterTopology& topology,
    const MediaGraphPlanningPolicy& policy)
{
    MediaGraphPlannerResult result;

    auto deployment = MediaGraphDeploymentPlanner::plan(graph, topology, policy);
    if (!deployment) {
        return ::media::Result<MediaGraphPlannerResult>::failure(deployment.error());
    }

    result.deploymentPlan = std::move(deployment.value().plan);
    result.report.append(deployment.value().report);

    auto gpu = MediaGraphGpuPlanner::plan(graph, policy);
    if (!gpu) {
        return ::media::Result<MediaGraphPlannerResult>::failure(gpu.error());
    }

    result.gpuCommands = std::move(gpu.value().commands);
    result.report.append(gpu.value().report);

    auto mesh = MediaGraphMeshPlanner::plan(graph, result.deploymentPlan, policy);
    if (!mesh) {
        return ::media::Result<MediaGraphPlannerResult>::failure(mesh.error());
    }

    result.meshRoutes = std::move(mesh.value().routes);
    result.report.append(mesh.value().report);
    result.report.info("planner", "graph planning completed");

    return ::media::Result<MediaGraphPlannerResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
