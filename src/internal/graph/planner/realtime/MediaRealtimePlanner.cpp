#include "internal/graph/planner/realtime/MediaRealtimePlanner.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status validateOptions(const MediaRealtimePlannerOptions& options)
{
    if (options.edgeNodeId.empty() ||
        options.workerNodeId.empty() ||
        options.host.empty() ||
        options.zone.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimePlanner requires explicit node ids, host, and zone"));
    }
    if (options.basePort <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimePlanner requires positive base port"));
    }
    if (options.targetLatencyUs <= 0 || options.maxLatencyUs < options.targetLatencyUs) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimePlanner requires valid latency bounds"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaRealtimePlannerResult> MediaRealtimePlanner::plan(
    const MediaGraph& graph,
    const MediaRealtimePlannerOptions& options)
{
    if (auto status = validateOptions(options); !status) {
        return ::media::Result<MediaRealtimePlannerResult>::failure(status.error());
    }

    MediaRealtimePlannerResult result;
    result.topology = buildTopology(options);
    result.policy = buildPolicy(options);

    auto planned = MediaGraphPlanner::plan(graph, result.topology, result.policy);
    if (!planned) {
        return ::media::Result<MediaRealtimePlannerResult>::failure(planned.error());
    }

    result.plannerResult = std::move(planned).value();
    result.plannerResult.report.info("realtime-planner", "realtime graph planning completed");
    return ::media::Result<MediaRealtimePlannerResult>::success(std::move(result));
}

MediaGraphClusterTopology MediaRealtimePlanner::buildTopology(const MediaRealtimePlannerOptions& options)
{
    MediaGraphClusterTopology topology;

    MediaGraphClusterNode edge;
    edge.address.nodeId = options.edgeNodeId;
    edge.address.host = options.host;
    edge.address.port = options.basePort;
    edge.address.zone = options.zone;
    edge.role = MediaGraphClusterNodeRole::Edge;
    edge.available = true;
    edge.weight = 1;
    topology.addNode(std::move(edge));

    MediaGraphClusterNode worker;
    worker.address.nodeId = options.workerNodeId;
    worker.address.host = options.host;
    worker.address.port = options.basePort + 1;
    worker.address.zone = options.zone;
    worker.role = MediaGraphClusterNodeRole::Worker;
    worker.available = true;
    worker.weight = 1;
    topology.addNode(std::move(worker));

    return topology;
}

MediaGraphPlanningPolicy MediaRealtimePlanner::buildPolicy(const MediaRealtimePlannerOptions& options)
{
    MediaGraphPlanningPolicy policy;
    policy.placementStrategy = MediaGraphPlacementStrategy::PreferEdgeForIo;
    policy.enableDistributedExecution = true;
    policy.enableGpuPlanning = options.enableGpuPlanning;
    policy.enableMeshPlanning = options.enableMeshPlanning;
    policy.keepSourceAndSinkOnEdge = true;

    policy.threadingPolicy.mode = MediaThreadingMode::PerNodeWorker;
    policy.threadingPolicy.priority = MediaThreadPriority::High;
    policy.threadingPolicy.collectWorkerMetrics = true;

    policy.optimizationPolicy.level = MediaGraphOptimizationLevel::Realtime;
    policy.optimizationPolicy.enableNodeFusion = options.enableNodeFusion;
    policy.optimizationPolicy.enableRedundantTransferElimination = true;
    policy.optimizationPolicy.enableQueuePolicyTuning = true;
    policy.optimizationPolicy.enableBackpressurePlanning = true;
    policy.optimizationPolicy.enableDiagnosticReport = true;

    policy.optimizationPolicy.latencyPolicy.mode = MediaLatencyMode::Realtime;
    policy.optimizationPolicy.latencyPolicy.targetLatencyUs = options.targetLatencyUs;
    policy.optimizationPolicy.latencyPolicy.maxLatencyUs = options.maxLatencyUs;
    policy.optimizationPolicy.latencyPolicy.maxJitterUs = options.targetLatencyUs / 2;
    policy.optimizationPolicy.latencyPolicy.enablePacing = true;
    policy.optimizationPolicy.latencyPolicy.dropLateFrames = true;
    policy.optimizationPolicy.latencyPolicy.preferKeyFrameRecovery = true;

    policy.optimizationPolicy.zeroCopyPolicy.mode = options.preferZeroCopy
        ? MediaZeroCopyMode::Prefer
        : MediaZeroCopyMode::Disabled;
    policy.optimizationPolicy.zeroCopyPolicy.allowHardwareMapping = true;
    policy.optimizationPolicy.zeroCopyPolicy.allowSoftwareTransfer = !options.preferZeroCopy;
    policy.zeroCopyPolicy = policy.optimizationPolicy.zeroCopyPolicy;

    return policy;
}

} // namespace media::ffmpeg::graph
