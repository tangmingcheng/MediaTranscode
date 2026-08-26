#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include "internal/graph/core/MediaGraphTopology.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/context/MediaGraphPayloadCreditWakeupHub.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <sstream>
#include <algorithm>
#include <new>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

MediaGraphExecutionContext::~MediaGraphExecutionContext()
{
    shutdownAvSyncGroups();
}

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

    if (graph.payloadCreditPlan()) {
        if (!graph.payloadCreditPlan()->isCompleteAndValid()) {
            reset();
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MediaGraphExecutionContext rejects an incomplete payload credit producer registry"));
        }
        auto ledger = MediaGraphPayloadCreditLedger::create(
            *graph.payloadCreditPlan());
        if (!ledger) {
            reset();
            return ::media::Status::failure(ledger.error());
        }
        m_payloadCreditLedger = std::move(ledger).value();
        m_payloadCreditWakeupHub =
            std::make_shared<MediaGraphPayloadCreditWakeupHub>();
        for (const auto& [node, wakeup] : m_nodeWakeups) {
            (void)node;
            m_payloadCreditWakeupHub->add(wakeup);
        }
        m_payloadCreditLedger->setReleaseObserver(
            m_payloadCreditWakeupHub);
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
    shutdownAvSyncGroups();
    m_graph = nullptr;
    m_channels.clear();
    if (m_payloadCreditWakeupHub) m_payloadCreditWakeupHub->interrupt();
    m_payloadCreditLedger.reset();
    m_payloadCreditWakeupHub.reset();
    m_executionOrder.clear();
    m_nodeWakeups.clear();
    m_compiled = false;
    mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

std::shared_ptr<MediaGraphPayloadCreditLedger>
MediaGraphExecutionContext::payloadCreditLedger() const noexcept
{
    return m_payloadCreditLedger;
}

::media::Result<MediaGraphPayloadReservation>
MediaGraphExecutionContext::reservePayload(
    MediaNodeId producer,
    MediaStreamKind streamKind,
    MediaPayloadKind payloadKind) noexcept
{
    auto strategy = reservePayloadBatch(
        producer, streamKind, payloadKind, {});
    if (!strategy) {
        return ::media::Result<MediaGraphPayloadReservation>::failure(
            strategy.error());
    }
    auto reservations = std::move(strategy).value();
    return ::media::Result<MediaGraphPayloadReservation>::success(
        std::move(reservations.front()));
}

::media::Result<std::vector<MediaGraphPayloadReservation>>
MediaGraphExecutionContext::reservePayloadBatch(
    MediaNodeId producer,
    MediaStreamKind streamKind,
    MediaPayloadKind payloadKind,
    std::span<const std::uint64_t> actualBytes) noexcept
{
    using Result =
        ::media::Result<std::vector<MediaGraphPayloadReservation>>;
    if (!m_payloadCreditLedger) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "runtime graph has no activated payload credit ledger"));
    }
    const auto& strategies = m_payloadCreditLedger->plan().producers;
    const MediaGraphPayloadProducerStrategy* selected = nullptr;
    for (const auto& strategy : strategies) {
        if (strategy.nodeId != producer ||
            strategy.payloadKind != payloadKind ||
            (streamKind != MediaStreamKind::Any &&
             strategy.streamKind != streamKind)) {
            continue;
        }
        if (!selected || strategy.maximumReservationBytes >
                selected->maximumReservationBytes) {
            selected = &strategy;
        }
    }
    if (!selected) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "runtime payload producer is absent from the final DAG registry"));
    }
    const bool accountsBytes = selected->accounting ==
        MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject;
    const std::uint64_t defaultBytes = accountsBytes
        ? selected->maximumReservationBytes : 0;
    std::vector<std::uint64_t> ledgerBytes;
    try {
        ledgerBytes.reserve(actualBytes.empty() ? 1 : actualBytes.size());
        if (actualBytes.empty()) {
            ledgerBytes.push_back(defaultBytes);
        } else {
            for (const auto bytes : actualBytes) {
                if (bytes == 0 || bytes > selected->maximumReservationBytes) {
                    return Result::failure(::media::ErrorInfo::invalidArgument(
                        "runtime payload batch exceeds its prepared single-unit bound"));
                }
                ledgerBytes.push_back(accountsBytes ? bytes : 0);
            }
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "runtime payload batch credit request"));
    }
    auto leases = m_payloadCreditLedger->tryReserveBatch(ledgerBytes);
    if (!leases) return Result::failure(leases.error());
    try {
        std::vector<MediaGraphPayloadReservation> reservations;
        reservations.reserve(leases.value().size());
        for (auto& lease : leases.value()) {
            reservations.emplace_back(
                selected->accounting, selected->maximumReservationBytes,
                std::move(lease));
        }
        return Result::success(std::move(reservations));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "runtime payload reservation identities"));
    }
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
    return *sharedNodeWakeup(nodeId);
}

std::shared_ptr<MediaNodeWakeup>
MediaGraphExecutionContext::sharedNodeWakeup(MediaNodeId nodeId)
{
    auto& wakeup = m_nodeWakeups[nodeId.value];
    if (!wakeup) {
        wakeup = std::make_shared<MediaNodeWakeup>();
    }
    return wakeup;
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

void MediaGraphExecutionContext::shutdownAvSyncGroups() noexcept
{
    interruptNodeWakeups();
    m_avSyncGroups.clear();
}

::media::Status MediaGraphExecutionContext::registerAvSyncGroup(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
    std::shared_ptr<MediaAvEpochTransitionService> transitionService)
{
    return m_avSyncGroups.registerGroup(
        std::move(key), std::move(plan), std::move(clock),
        std::move(sharedNtpEpoch), std::move(transitionService));
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
