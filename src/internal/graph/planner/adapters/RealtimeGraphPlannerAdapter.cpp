#include "internal/graph/planner/adapters/RealtimeGraphPlannerAdapter.h"

#include "internal/graph/planner/MediaGraphPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

const char* RealtimeGraphPlannerAdapter::name() const noexcept
{
    return "realtime";
}

::media::Result<MediaGraphPlannerAdapterResult> RealtimeGraphPlannerAdapter::plan(
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

MediaGraphClusterTopology RealtimeGraphPlannerAdapter::buildTopology(const MediaGraphPlannerAdapterOptions& options)
{
    MediaGraphClusterTopology topology;

    MediaGraphClusterNode edge;
    edge.address.nodeId = options.edgeNodeId.empty() ? "edge" : options.edgeNodeId;
    edge.address.host = options.host.empty() ? "127.0.0.1" : options.host;
    edge.address.port = options.basePort > 0 ? options.basePort : 19000;
    edge.address.zone = options.zone.empty() ? "realtime" : options.zone;
    edge.role = MediaGraphClusterNodeRole::Edge;
    edge.available = true;
    edge.weight = 1;
    topology.addNode(std::move(edge));

    MediaGraphClusterNode worker;
    worker.address.nodeId = options.workerNodeId.empty() ? "worker" : options.workerNodeId;
    worker.address.host = options.host.empty() ? "127.0.0.1" : options.host;
    worker.address.port = (options.basePort > 0 ? options.basePort : 19000) + 1;
    worker.address.zone = options.zone.empty() ? "realtime" : options.zone;
    worker.role = MediaGraphClusterNodeRole::Worker;
    worker.available = true;
    worker.weight = 1;
    topology.addNode(std::move(worker));

    return topology;
}

MediaGraphPlanningPolicy RealtimeGraphPlannerAdapter::buildPolicy(const MediaGraphPlannerAdapterOptions& options)
{
    MediaGraphPlanningPolicy policy;
    policy.placementStrategy = MediaGraphPlacementStrategy::PreferEdgeForIo;
    policy.enableDistributedExecution = true;
    policy.enableGpuPlanning = options.enableGpuPlanning;
    policy.enableMeshPlanning = options.enableMeshPlanning;
    policy.keepSourceAndSinkOnEdge = true;

    policy.threadingPolicy.mode = MediaThreadingMode::PerNodeWorker;
    policy.threadingPolicy.priority = MediaThreadPriority::High;
    policy.threadingPolicy.idleSleepMs = 0;
    policy.threadingPolicy.maxIdleSpins = 1;

    policy.optimizationPolicy.level = MediaGraphOptimizationLevel::Realtime;
    policy.optimizationPolicy.enableNodeFusion = true;
    policy.optimizationPolicy.enableQueuePolicyTuning = true;
    policy.optimizationPolicy.enableBackpressurePlanning = true;
    policy.optimizationPolicy.latencyPolicy.mode = MediaLatencyMode::Realtime;
    policy.optimizationPolicy.latencyPolicy.targetLatencyUs = 50000;
    policy.optimizationPolicy.latencyPolicy.maxLatencyUs = 150000;
    policy.optimizationPolicy.latencyPolicy.enablePacing = true;
    policy.optimizationPolicy.latencyPolicy.dropLateFrames = true;

    policy.optimizationPolicy.zeroCopyPolicy.mode = options.preferZeroCopy
        ? MediaZeroCopyMode::Prefer
        : MediaZeroCopyMode::Disabled;
    policy.zeroCopyPolicy = policy.optimizationPolicy.zeroCopyPolicy;

    return policy;
}

} // namespace media::ffmpeg::graph
