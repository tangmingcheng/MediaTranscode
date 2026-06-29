#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <deque>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

::media::Status MediaGraphExecutionContext::compile(const MediaGraph& graph)
{
    const bool diagnosticsEnabled = m_diagnosticsEnabled;
    reset();
    m_diagnosticsEnabled = diagnosticsEnabled;
    mediaGraphDiagnosticSetGlobalEnabled(m_diagnosticsEnabled);

    auto report = MediaGraphValidation::validate(graph);
    if (!report.ok()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaGraphExecutionContext compile failed: graph validation has " +
                std::to_string(report.errorCount()) + " error(s)"));
    }

    auto channelStatus = buildChannels(graph);
    if (!channelStatus) {
        reset();
        return channelStatus;
    }

    auto orderStatus = buildExecutionOrder(graph);
    if (!orderStatus) {
        reset();
        return orderStatus;
    }

    m_graph = &graph;
    m_compiled = true;

    std::ostringstream out;
    out << "compiled nodes=" << graph.nodeCount()
        << " edges=" << graph.edgeCount()
        << " channels=" << m_channels.channels().size()
        << " execution_order=";
    bool first = true;
    for (MediaNodeId nodeId : m_executionOrder) {
        if (!first) {
            out << "->";
        }
        first = false;
        out << nodeId.value;
    }
    mediaGraphDiagnosticLog(m_diagnosticsEnabled, MediaGraphDiagnosticPhase::RuntimeLifecycle, out.str());

    return ::media::Status::success();
}

void MediaGraphExecutionContext::reset()
{
    m_graph = nullptr;
    m_channels.clear();
    m_executionOrder.clear();
    m_compiled = false;
    mediaGraphDiagnosticSetGlobalEnabled(m_diagnosticsEnabled);
}

void MediaGraphExecutionContext::setDiagnosticsEnabled(bool enabled) noexcept
{
    m_diagnosticsEnabled = enabled;
    mediaGraphDiagnosticSetGlobalEnabled(enabled);
}

bool MediaGraphExecutionContext::diagnosticsEnabled() const noexcept
{
    return m_diagnosticsEnabled;
}

bool MediaGraphExecutionContext::compiled() const noexcept
{
    return m_compiled;
}

const MediaGraph* MediaGraphExecutionContext::graph() const noexcept
{
    return m_graph;
}

MediaChannelRegistry& MediaGraphExecutionContext::channels() noexcept
{
    return m_channels;
}

const MediaChannelRegistry& MediaGraphExecutionContext::channels() const noexcept
{
    return m_channels;
}

const std::vector<MediaNodeId>& MediaGraphExecutionContext::executionOrder() const noexcept
{
    return m_executionOrder;
}

MediaChannel* MediaGraphExecutionContext::findInputChannel(MediaNodeId nodeId, const std::string& portName)
{
    return const_cast<MediaChannel*>(
        static_cast<const MediaGraphExecutionContext*>(this)->findInputChannel(nodeId, portName));
}

const MediaChannel* MediaGraphExecutionContext::findInputChannel(MediaNodeId nodeId, const std::string& portName) const
{
    if (!m_graph) {
        return nullptr;
    }

    const MediaPort* port = m_graph->findInputPort(nodeId, portName);
    if (!port) {
        return nullptr;
    }

    for (const auto& edge : m_graph->edges()) {
        if (edge.to.portId == port->id) {
            return m_channels.findByEdge(edge.id);
        }
    }

    return nullptr;
}

MediaChannel* MediaGraphExecutionContext::findOutputChannel(MediaNodeId nodeId, const std::string& portName)
{
    return const_cast<MediaChannel*>(
        static_cast<const MediaGraphExecutionContext*>(this)->findOutputChannel(nodeId, portName));
}

const MediaChannel* MediaGraphExecutionContext::findOutputChannel(MediaNodeId nodeId, const std::string& portName) const
{
    if (!m_graph) {
        return nullptr;
    }

    const MediaPort* port = m_graph->findOutputPort(nodeId, portName);
    if (!port) {
        return nullptr;
    }

    for (const auto& edge : m_graph->edges()) {
        if (edge.from.portId == port->id) {
            return m_channels.findByEdge(edge.id);
        }
    }

    return nullptr;
}

std::vector<MediaChannel*> MediaGraphExecutionContext::inputChannels(MediaNodeId nodeId)
{
    std::vector<MediaChannel*> result;
    if (!m_graph) {
        return result;
    }

    for (const auto& edge : m_graph->edges()) {
        if (edge.to.nodeId == nodeId) {
            if (MediaChannel* channel = m_channels.findByEdge(edge.id)) {
                result.push_back(channel);
            }
        }
    }

    return result;
}

std::vector<MediaChannel*> MediaGraphExecutionContext::outputChannels(MediaNodeId nodeId)
{
    std::vector<MediaChannel*> result;
    if (!m_graph) {
        return result;
    }

    for (const auto& edge : m_graph->edges()) {
        if (edge.from.nodeId == nodeId) {
            if (MediaChannel* channel = m_channels.findByEdge(edge.id)) {
                result.push_back(channel);
            }
        }
    }

    return result;
}

::media::Status MediaGraphExecutionContext::buildChannels(const MediaGraph& graph)
{
    for (const auto& edge : graph.edges()) {
        auto result = m_channels.createChannel(edge);
        if (!result) {
            return ::media::Status::failure(result.error());
        }

        if (MediaChannel* channel = result.value()) {
            mediaGraphDiagnosticLog(m_diagnosticsEnabled,
                                    MediaGraphDiagnosticPhase::RuntimeChannel,
                                    std::string("create ") + mediaGraphDiagnosticDescribeChannel(*channel));
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphExecutionContext::buildExecutionOrder(const MediaGraph& graph)
{
    std::unordered_map<uint32_t, int> indegree;
    std::unordered_map<uint32_t, std::vector<uint32_t>> adjacency;
    std::unordered_map<uint32_t, MediaNodeId> ids;

    for (const auto& node : graph.nodes()) {
        indegree[node.id.value] = 0;
        ids[node.id.value] = node.id;
    }

    for (const auto& edge : graph.edges()) {
        adjacency[edge.from.nodeId.value].push_back(edge.to.nodeId.value);
        ++indegree[edge.to.nodeId.value];
    }

    std::deque<uint32_t> ready;
    for (const auto& item : indegree) {
        if (item.second == 0) {
            ready.push_back(item.first);
        }
    }

    while (!ready.empty()) {
        uint32_t current = ready.front();
        ready.pop_front();

        m_executionOrder.push_back(ids[current]);

        for (uint32_t next : adjacency[current]) {
            --indegree[next];
            if (indegree[next] == 0) {
                ready.push_back(next);
            }
        }
    }

    if (m_executionOrder.size() != graph.nodes().size()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphExecutionContext compile failed: graph is cyclic"));
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
