#include "internal/graph/planner/local/MediaLocalPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaLocalPlannerResult> MediaLocalPlanner::plan(
    const MediaGraph& graph,
    const MediaLocalPlannerOptions& options)
{
    MediaLocalPlannerResult result;
    result.topology = buildTopology(options);
    result.policy = buildPolicy(options);

    auto planned = MediaGraphPlanner::plan(graph, result.topology, result.policy);
    if (!planned) {
        return ::media::Result<MediaLocalPlannerResult>::failure(planned.error());
    }

    result.plannerResult = std::move(planned).value();
    result.plannerResult.report.info("local-planner", "local graph planning completed");
    return ::media::Result<MediaLocalPlannerResult>::success(std::move(result));
}

MediaGraphClusterTopology MediaLocalPlanner::buildTopology(const MediaLocalPlannerOptions& options)
{
    MediaGraphClusterTopology topology;

    MediaGraphClusterNode local;
    local.address.nodeId = options.nodeId.empty() ? "local" : options.nodeId;
    local.address.host = options.host.empty() ? "127.0.0.1" : options.host;
    local.address.port = options.port > 0 ? options.port : 19000;
    local.address.zone = options.zone.empty() ? "local" : options.zone;
    local.role = MediaGraphClusterNodeRole::Worker;
    local.available = true;
    local.weight = 1;

    topology.addNode(std::move(local));
    return topology;
}

MediaGraphPlanningPolicy MediaLocalPlanner::buildPolicy(const MediaLocalPlannerOptions& options)
{
    MediaGraphPlanningPolicy policy;
    policy.placementStrategy = MediaGraphPlacementStrategy::SingleNode;
    policy.enableDistributedExecution = false;
    policy.enableGpuPlanning = options.enableGpuPlanning;
    policy.enableMeshPlanning = options.enableMeshPlanning;
    policy.keepSourceAndSinkOnEdge = false;

    policy.threadingPolicy.mode = MediaThreadingMode::SingleThreaded;
    policy.threadingPolicy.priority = MediaThreadPriority::Normal;

    policy.optimizationPolicy.level = MediaGraphOptimizationLevel::Safe;
    policy.optimizationPolicy.enableNodeFusion = false;
    policy.optimizationPolicy.enableRedundantTransferElimination = true;
    policy.optimizationPolicy.enableQueuePolicyTuning = true;
    policy.optimizationPolicy.enableBackpressurePlanning = true;
    policy.optimizationPolicy.enableDiagnosticReport = true;

    policy.optimizationPolicy.latencyPolicy.mode = MediaLatencyMode::Balanced;
    policy.optimizationPolicy.latencyPolicy.enablePacing = false;
    policy.optimizationPolicy.latencyPolicy.dropLateFrames = false;

    policy.optimizationPolicy.zeroCopyPolicy.mode = options.preferZeroCopy
        ? MediaZeroCopyMode::Prefer
        : MediaZeroCopyMode::Disabled;
    policy.zeroCopyPolicy = policy.optimizationPolicy.zeroCopyPolicy;

    return policy;
}

} // namespace media::ffmpeg::graph
