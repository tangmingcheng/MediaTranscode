#include "internal/graph/planner/MediaGraphDeploymentPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphDeploymentPlannerResult> MediaGraphDeploymentPlanner::plan(
    const MediaGraph& graph,
    const MediaGraphClusterTopology& topology,
    const MediaGraphPlanningPolicy& policy)
{
    if (graph.empty()) {
        return ::media::Result<MediaGraphDeploymentPlannerResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphDeploymentPlanner failed: graph is empty"));
    }

    if (topology.size() == 0) {
        return ::media::Result<MediaGraphDeploymentPlannerResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphDeploymentPlanner failed: topology is empty"));
    }

    if (policy.enableDistributedExecution) {
        return ::media::Result<MediaGraphDeploymentPlannerResult>::failure(
            ::media::ErrorInfo::unsupported(
                "MediaGraphDeploymentPlanner distributed execution is unsupported"));
    }

    if (topology.size() != 1) {
        return ::media::Result<MediaGraphDeploymentPlannerResult>::failure(
            ::media::ErrorInfo::unsupported(
                "MediaGraphDeploymentPlanner requires a single-node topology when distributed execution is disabled"));
    }

    MediaGraphDeploymentPlannerResult result;
    std::size_t roundRobinIndex = 0;

    for (const MediaNode& node : graph.nodes()) {
        const MediaGraphClusterNode* selected = selectNode(node, topology, policy, roundRobinIndex);
        if (!selected) {
            result.report.error("deployment", "no available cluster node for graph node: " + node.name);
            return ::media::Result<MediaGraphDeploymentPlannerResult>::failure(
                ::media::ErrorInfo::notInitialized("MediaGraphDeploymentPlanner failed: no available cluster node"));
        }

        result.plan.assign(node.id, selected->address);
        result.report.info("deployment", "assigned graph node " + node.name + " to cluster node " + selected->address.nodeId);
    }

    return ::media::Result<MediaGraphDeploymentPlannerResult>::success(std::move(result));
}

const MediaGraphClusterNode* MediaGraphDeploymentPlanner::selectNode(const MediaNode& graphNode,
                                                                     const MediaGraphClusterTopology& topology,
                                                                     const MediaGraphPlanningPolicy& policy,
                                                                     std::size_t& roundRobinIndex)
{
    if (policy.keepSourceAndSinkOnEdge && isIoNode(graphNode.kind)) {
        if (const MediaGraphClusterNode* edge = firstAvailableRole(topology, MediaGraphClusterNodeRole::Edge)) {
            return edge;
        }
    }

    const std::vector<MediaGraphClusterNode> workers = topology.workers();
    if (!workers.empty()) {
        const std::size_t index = roundRobinIndex % workers.size();
        ++roundRobinIndex;
        return topology.findNode(workers[index].address.nodeId);
    }

    if (const MediaGraphClusterNode* coordinator = firstAvailableRole(topology, MediaGraphClusterNodeRole::Coordinator)) {
        return coordinator;
    }

    const std::vector<MediaGraphClusterNode> allNodes = topology.nodes();
    for (const MediaGraphClusterNode& node : allNodes) {
        if (node.available) {
            return topology.findNode(node.address.nodeId);
        }
    }

    return nullptr;
}

const MediaGraphClusterNode* MediaGraphDeploymentPlanner::firstAvailableRole(const MediaGraphClusterTopology& topology,
                                                                              MediaGraphClusterNodeRole role)
{
    const std::vector<MediaGraphClusterNode> nodes = topology.nodes();
    for (const MediaGraphClusterNode& node : nodes) {
        if (node.available && node.role == role) {
            return topology.findNode(node.address.nodeId);
        }
    }
    return nullptr;
}

bool MediaGraphDeploymentPlanner::isIoNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput:
    case MediaNodeKind::RealtimeInput:
    case MediaNodeKind::FileOutput:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
        return true;
    default:
        return false;
    }
}

} // namespace media::ffmpeg::graph
