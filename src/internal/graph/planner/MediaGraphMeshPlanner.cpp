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

    for (const MediaEdge& edge : graph.edges()) {
        const MediaGraphDeploymentAssignment* from = deploymentPlan.find(edge.from.nodeId);
        const MediaGraphDeploymentAssignment* to = deploymentPlan.find(edge.to.nodeId);

        if (!from || !to) {
            result.report.warning("mesh-planner", "skipped edge without complete deployment assignment: " + edge.name);
            continue;
        }

        MediaMeshRoute route;
        route.routeId = edge.name.empty() ? ("edge-" + std::to_string(edge.id.value)) : edge.name;
        route.kind = routeKindFor(*from, *to);
        route.source = from->address;
        route.targets.push_back(to->address);

        if (route.valid()) {
            result.routes.push_back(std::move(route));
            result.report.info("mesh-planner", "planned route for edge: " + edge.name);
        }
    }

    if (result.routes.empty()) {
        result.report.info("mesh-planner", "no mesh routes planned");
    }

    return ::media::Result<MediaGraphMeshPlannerResult>::success(std::move(result));
}

MediaMeshRouteKind MediaGraphMeshPlanner::routeKindFor(const MediaGraphDeploymentAssignment& from,
                                                       const MediaGraphDeploymentAssignment& to) noexcept
{
    if (from.clusterNodeId == to.clusterNodeId) {
        return MediaMeshRouteKind::Local;
    }

    return MediaMeshRouteKind::Remote;
}

} // namespace media::ffmpeg::graph
