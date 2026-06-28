#include "internal/graph/planner/adapters/LocalGraphPlannerAdapter.h"

#include "internal/graph/planner/MediaGraphPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

const char* LocalGraphPlannerAdapter::name() const noexcept
{
    return "local";
}

::media::Result<MediaGraphPlannerAdapterResult> LocalGraphPlannerAdapter::plan(
    const MediaGraph& graph,
    const MediaGraphPlannerAdapterOptions& options) const
{
    MediaGraphPlannerAdapterResult result;
    result.topology = buildTopology(options);
    result.policy = buildPolicy(options);

    auto planned = MediaGraphPlanner::plan(graph, result.topology, result.policy);
    if (!planned) {
        return ::media::Result<MediaGraphPlannerAdapterResult>::failure(planned.error());
    }

    result.plannerResult = std::move(planned).value();
    return ::media::Result<MediaGraphPlannerAdapterResult>::success(std::move(result));
}

MediaGraphClusterTopology LocalGraphPlannerAdapter::buildTopology(const MediaGraphPlannerAdapterOptions& options)
{
    MediaGraphClusterTopology topology;

    MediaGraphClusterNode local;
    local.address.nodeId = options.localNodeId.empty() ? "local" : options.localNodeId;
    local.address.host = options.host.empty() ? "127.0.0.1" : options.host;
    local.address.port = options.basePort > 0 ? options.basePort : 19000;
    local.address.zone = options.zone;
    local.role = MediaGraphClusterNodeRole::Worker;
    local.available = true;
    local.weight = 1;

    topology.addNode(std::move(local));
    return topology;
}

MediaGraphPlanningPolicy LocalGraphPlannerAdapter::buildPolicy(const MediaGraphPlannerAdapterOptions& options)
{
    MediaGraphPlanningPolicy policy;
    policy.placementStrategy = MediaGraphPlacementStrategy::SingleNode;
    policy.enableDistributedExecution = false;
    policy.enableGpuPlanning = options.enableGpuPlanning;
    policy.enableMeshPlanning = options.enableMeshPlanning;
    policy.keepSourceAndSinkOnEdge = false;

    policy.threadingPolicy.mode = MediaThreadingMode::SingleThreaded;
    policy.optimizationPolicy.level = MediaGraphOptimizationLevel::Safe;
    policy.optimizationPolicy.enableDiagnosticReport = true;
    policy.optimizationPolicy.zeroCopyPolicy.mode = options.preferZeroCopy
        ? MediaZeroCopyMode::Prefer
        : MediaZeroCopyMode::Disabled;
    policy.zeroCopyPolicy = policy.optimizationPolicy.zeroCopyPolicy;

    return policy;
}

} // namespace media::ffmpeg::graph
