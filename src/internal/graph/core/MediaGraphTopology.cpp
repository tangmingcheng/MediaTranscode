#include "internal/graph/core/MediaGraphTopology.h"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using NodeKey = uint32_t;

struct TopologyState {
    std::unordered_map<NodeKey, int> indegree;
    std::unordered_map<NodeKey, int> outdegree;
    std::unordered_map<NodeKey, std::vector<NodeKey>> adjacency;
    std::unordered_map<NodeKey, MediaNodeId> ids;
};

TopologyState buildState(const MediaGraph& graph)
{
    TopologyState state;

    for (const MediaNode& node : graph.nodes()) {
        if (!node.id) {
            continue;
        }

        const NodeKey key = node.id.value;
        state.indegree[key] = 0;
        state.outdegree[key] = 0;
        state.ids[key] = node.id;
    }

    for (const MediaEdge& edge : graph.edges()) {
        if (!edge.isValid()) {
            continue;
        }

        const NodeKey from = edge.from.nodeId.value;
        const NodeKey to = edge.to.nodeId.value;
        if (state.ids.find(from) == state.ids.end() || state.ids.find(to) == state.ids.end()) {
            continue;
        }

        state.adjacency[from].push_back(to);
        ++state.indegree[to];
        ++state.outdegree[from];
    }

    return state;
}

void pushInitialSources(const MediaGraph& graph,
                        const TopologyState& state,
                        std::deque<NodeKey>& ready,
                        MediaGraphTopologyResult& result)
{
    for (const MediaNode& node : graph.nodes()) {
        if (!node.id) {
            continue;
        }

        const NodeKey key = node.id.value;
        const auto indegreeIt = state.indegree.find(key);
        if (indegreeIt != state.indegree.end() && indegreeIt->second == 0) {
            ready.push_back(key);
            result.sources.push_back(node.id);
        }
    }
}

void appendSinks(const MediaGraph& graph,
                 const TopologyState& state,
                 MediaGraphTopologyResult& result)
{
    for (const MediaNode& node : graph.nodes()) {
        if (!node.id) {
            continue;
        }

        const NodeKey key = node.id.value;
        const auto outdegreeIt = state.outdegree.find(key);
        if (outdegreeIt != state.outdegree.end() && outdegreeIt->second == 0) {
            result.sinks.push_back(node.id);
        }
    }
}

void appendLevels(const std::unordered_map<NodeKey, int>& nodeLevels,
                  const std::vector<MediaNodeId>& order,
                  MediaGraphTopologyResult& result)
{
    int maxLevel = 0;
    for (const auto& item : nodeLevels) {
        maxLevel = std::max(maxLevel, item.second);
    }

    result.levels.clear();
    result.levels.resize(static_cast<std::size_t>(maxLevel + 1));
    for (MediaNodeId nodeId : order) {
        const auto levelIt = nodeLevels.find(nodeId.value);
        if (levelIt == nodeLevels.end()) {
            continue;
        }
        result.levels[static_cast<std::size_t>(levelIt->second)].push_back(nodeId);
    }
}

} // namespace

::media::Result<MediaGraphTopologyResult> MediaGraphTopology::build(const MediaGraph& graph)
{
    MediaGraphTopologyResult result;
    TopologyState state = buildState(graph);
    std::unordered_map<NodeKey, int> nodeLevels;

    for (const auto& item : state.ids) {
        nodeLevels[item.first] = 0;
    }

    std::deque<NodeKey> ready;
    pushInitialSources(graph, state, ready, result);
    appendSinks(graph, state, result);

    while (!ready.empty()) {
        const NodeKey current = ready.front();
        ready.pop_front();

        auto idIt = state.ids.find(current);
        if (idIt == state.ids.end()) {
            continue;
        }

        result.order.push_back(idIt->second);

        const auto adjacencyIt = state.adjacency.find(current);
        if (adjacencyIt == state.adjacency.end()) {
            continue;
        }

        for (NodeKey next : adjacencyIt->second) {
            auto indegreeIt = state.indegree.find(next);
            if (indegreeIt == state.indegree.end()) {
                continue;
            }

            nodeLevels[next] = std::max(nodeLevels[next], nodeLevels[current] + 1);
            --indegreeIt->second;
            if (indegreeIt->second == 0) {
                ready.push_back(next);
            }
        }
    }

    if (result.order.size() != state.ids.size()) {
        return ::media::Result<MediaGraphTopologyResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphTopology build failed: graph is cyclic"));
    }

    appendLevels(nodeLevels, result.order, result);
    return ::media::Result<MediaGraphTopologyResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
