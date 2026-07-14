#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include "internal/graph/core/MediaGraphTopology.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphExecutionContext::compile(const MediaGraph& graph)
{
    const MediaGraphDiagnosticConfig diagnosticConfig = m_diagnosticConfig;
    reset();
    m_diagnosticConfig = diagnosticConfig;
    mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);

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
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Summary,
                            MediaGraphDiagnosticPhase::RuntimeLifecycle,
                            out.str());

    return ::media::Status::success();
}

void MediaGraphExecutionContext::reset()
{
    m_graph = nullptr;
    m_channels.clear();
    m_executionOrder.clear();
    m_nodeWakeups.clear();
    m_avSyncGroups.clear();
    m_compiled = false;
    mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

void MediaGraphExecutionContext::rebindCompiledGraph(const MediaGraph& graph) noexcept
{
    if (m_compiled) m_graph = &graph;
}

void MediaGraphExecutionContext::setDiagnosticsEnabled(bool enabled) noexcept
{
    m_diagnosticConfig.level = enabled ? MediaGraphDiagnosticLevel::State : MediaGraphDiagnosticLevel::Off;
    mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

bool MediaGraphExecutionContext::diagnosticsEnabled() const noexcept
{
    return m_diagnosticConfig.level != MediaGraphDiagnosticLevel::Off;
}

void MediaGraphExecutionContext::setDiagnosticConfig(MediaGraphDiagnosticConfig config) noexcept
{
    m_diagnosticConfig = config;
    mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

const MediaGraphDiagnosticConfig& MediaGraphExecutionContext::diagnosticConfig() const noexcept
{
    return m_diagnosticConfig;
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

MediaNodeWakeup& MediaGraphExecutionContext::nodeWakeup(MediaNodeId nodeId)
{
    auto& wakeup = m_nodeWakeups[nodeId.value];
    if (!wakeup) {
        wakeup = std::make_unique<MediaNodeWakeup>();
    }
    return *wakeup;
}

void MediaGraphExecutionContext::interruptNodeWakeups() noexcept
{
    for (auto& [nodeId, wakeup] : m_nodeWakeups) {
        (void)nodeId;
        if (wakeup) {
            wakeup->interrupt();
        }
    }
}

::media::Status MediaGraphExecutionContext::registerAvSyncGroup(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock)
{
    return m_avSyncGroups.registerGroup(
        std::move(key), std::move(plan), std::move(clock));
}

::media::Status MediaGraphExecutionContext::activatePlaybackEpoch(
    const MediaAvSyncGroupKey& key,
    MediaPlaybackEpoch epoch)
{
    return m_avSyncGroups.activatePlaybackEpoch(key, epoch);
}

::media::Status MediaGraphExecutionContext::activateNextPlaybackEpoch(
    const MediaAvSyncGroupKey& key,
    MediaPlaybackEpoch epoch)
{
    return m_avSyncGroups.activateNextPlaybackEpoch(key, epoch);
}

std::shared_ptr<MediaAvSyncGroupRuntime>
MediaGraphExecutionContext::findAvSyncGroup(
    const MediaAvSyncGroupKey& key) const noexcept
{
    return m_avSyncGroups.find(key);
}

::media::Status MediaGraphExecutionContext::buildChannels(const MediaGraph& graph)
{
    for (const auto& edge : graph.edges()) {
        auto result = m_channels.createChannel(edge);
        if (!result) {
            return ::media::Status::failure(result.error());
        }

        if (MediaChannel* channel = result.value()) {
            channel->setConsumerWakeup(nodeWakeup(edge.to.nodeId));
            channel->setProducerWakeup(nodeWakeup(edge.from.nodeId));
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                    MediaGraphDiagnosticPhase::RuntimeChannel,
                                    std::string("create ") + mediaGraphDiagnosticDescribeChannel(*channel));
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphExecutionContext::buildExecutionOrder(const MediaGraph& graph)
{
    auto topology = MediaGraphTopology::build(graph);
    if (!topology) {
        return ::media::Status::failure(topology.error());
    }

    m_executionOrder = std::move(topology).value().order;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
