#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include "internal/graph/core/MediaGraphTopology.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"
#include "internal/graph/runtime/lifecycle/MediaInputActivity.h"

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
    m_ownsGlobalDiagnostics = true;
    const MediaGraphDiagnosticConfig diagnosticConfig = m_diagnosticConfig;
    reset();
    m_diagnosticConfig = diagnosticConfig;
    if (m_ownsGlobalDiagnostics) mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);

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

    if (!graph.payloadCreditMode()) {
        reset();
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaGraphExecutionContext requires a typed payload credit mode"));
    }
    if (*graph.payloadCreditMode() ==
        MediaGraphPayloadCreditMode::RealtimeRequired) {
        if (!graph.payloadCreditPlan()) {
            reset();
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "realtime execution requires its installed payload credit plan"));
        }
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
    } else if (graph.payloadCreditPlan()) {
        reset();
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "non-realtime execution rejects a payload credit plan"));
    }

    try {
        m_inputActivity = std::make_shared<MediaInputActivity>();
    } catch (const std::bad_alloc&) {
        reset();
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("input activity evidence"));
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

::media::Result<MediaRuntimeSegmentOutputBinding>
MediaGraphExecutionContext::exportOutput(MediaPortId id) const
{
    using Result = ::media::Result<MediaRuntimeSegmentOutputBinding>;
    const auto* port = m_graph ? m_graph->findPort(id) : nullptr;
    if (!m_compiled || !port || !port->isOutput() ||
        std::find(m_executionOrder.begin(), m_executionOrder.end(), port->nodeId) == m_executionOrder.end())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Segment output must belong to an executing upstream node"));
    auto wakeup = findNodeWakeup(port->nodeId);
    auto exit = nodeExitToken(port->nodeId);
    if (!wakeup || !exit || !m_payloadCreditLedger)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Segment output has no prepared execution identity"));
    return Result::success(MediaRuntimeSegmentOutputBinding(
        *port, std::move(wakeup), std::move(exit), m_payloadCreditLedger));
}

::media::Status MediaGraphExecutionContext::compileSegment(
    std::shared_ptr<const MediaGraph> graph,
    std::span<const MediaNodeId> nodes,
    MediaGraphExecutionContext& session,
    std::span<const MediaRuntimeSegmentOutputBinding> upstreamInputs)
{
    if (!graph || nodes.empty() || !session.compiled() || !session.graph()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime segment requires a complete DAG snapshot and an active session"));
    }
    const auto report = MediaGraphValidation::validate(*graph);
    if (!report.ok() || !graph->payloadCreditPlan() ||
        !graph->payloadCreditPlan()->isCompleteAndValid() ||
        !session.payloadCreditLedger()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime segment requires validated topology and shared payload credits"));
    }
    std::vector<MediaNodeId> selected(nodes.begin(), nodes.end());
    const auto contains = [&selected](MediaNodeId id) {
        return std::find(selected.begin(), selected.end(), id) != selected.end();
    };
    const bool extracting = std::all_of(selected.begin(), selected.end(),
        [&session](MediaNodeId id) {
            const auto& order = session.executionOrder();
            return std::find(order.begin(), order.end(), id) != order.end();
        });
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (!graph->findNode(selected[i]) ||
            (!extracting && session.graph()->findNode(selected[i])) ||
            std::find(selected.begin(), selected.begin() + i, selected[i]) !=
                selected.begin() + i) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "runtime segment node IDs must be new, valid and unique"));
        }
    }
    auto topology = MediaGraphTopology::build(*graph);
    if (!topology) return ::media::Status::failure(topology.error());
    m_ownsGlobalDiagnostics = false;
    m_diagnosticConfig = session.diagnosticConfig();
    reset();
    m_graphOwner = std::move(graph);
    m_graph = m_graphOwner.get();
    m_ownsPayloadLedger = false;
    m_payloadCreditLedger = session.payloadCreditLedger();
    m_inputActivity = session.inputActivity();
    for (const auto id : selected) {
        if (extracting) {
            auto wakeup = session.findNodeWakeup(id);
            if (!wakeup) {
                reset();
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "initial segment node has no prepared wakeup"));
            }
            m_nodeWakeups.emplace(id.value, std::move(wakeup));
        } else sharedNodeWakeup(id);
        auto exitToken = extracting ? session.nodeExitToken(id)
            : std::make_shared<MediaGraphWorkerExitToken>();
        if (!exitToken) {
            reset();
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "initial segment node has no prepared exit token"));
        }
        m_nodeExitTokens.emplace(id.value, std::move(exitToken));
    }
    bool externalInput = false;
    for (const auto& edge : m_graph->edges()) {
        // The consumer segment owns each cross-segment channel. Outgoing edges
        // remain in the logical DAG and are instantiated by that consumer.
        if (!contains(edge.to.nodeId)) continue;
        const bool external = !contains(edge.from.nodeId);
        const MediaRuntimeSegmentOutputBinding* upstream = nullptr;
        if (external) {
            for (const auto& candidate : upstreamInputs) {
                if (candidate.m_port.id != edge.from.portId) continue;
                if (upstream) {
                    reset();
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                        "Segment input has duplicate upstream bindings"));
                }
                upstream = &candidate;
            }
            const auto* declared = m_graph->findPort(edge.from.portId);
            if (!upstream || !declared || upstream->m_port.nodeId != edge.from.nodeId ||
                upstream->m_ledger != session.payloadCreditLedger() || !upstream->m_wakeup ||
                !upstream->m_exit || upstream->m_port.name != declared->name ||
                upstream->m_port.streamKind != declared->streamKind ||
                upstream->m_port.edgeKind != declared->edgeKind ||
                upstream->m_port.payloadKind != declared->payloadKind) {
                reset();
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Segment input differs from its executing upstream endpoint"));
            }
        }
        if (extracting) {
            auto retained = session.channels().retainByEdge(edge.id);
            auto adopted = m_channels.adopt(std::move(retained));
            if (!adopted) { reset(); return adopted; }
            externalInput = externalInput || external;
            continue;
        }
        auto created = m_channels.createChannel(edge);
        if (!created) { reset(); return ::media::Status::failure(created.error()); }
        created.value()->setConsumerWakeup(sharedNodeWakeup(edge.to.nodeId));
        created.value()->setProducerWakeup(external
            ? upstream->m_wakeup
            : sharedNodeWakeup(edge.from.nodeId));
        externalInput = externalInput || external;
    }
    if (!externalInput) {
        reset();
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime output segment requires an explicit cross-segment input edge"));
    }
    for (const auto id : topology.value().order) {
        if (contains(id)) m_executionOrder.push_back(id);
    }
    m_compiled = true;
    return ::media::Status::success();
}

void MediaGraphExecutionContext::detachExecutionNodes(std::span<const MediaNodeId> nodes)
{
    const auto contains = [nodes](MediaNodeId id) {
        return std::find(nodes.begin(), nodes.end(), id) != nodes.end();
    };
    std::erase_if(m_executionOrder, contains);
    for (const auto id : nodes) {
        m_nodeWakeups.erase(id.value);
        m_nodeExitTokens.erase(id.value);
    }
    for (const auto& edge : m_graph->edges()) {
        if (contains(edge.from.nodeId) && contains(edge.to.nodeId))
            m_channels.removeByEdge(edge.id);
    }
}

void MediaGraphExecutionContext::cancelUnstartedExecution() noexcept
{
    for (const auto id : m_executionOrder) {
        const auto found = m_nodeExitTokens.find(id.value);
        if (found != m_nodeExitTokens.end())
            found->second->m_cancelledBeforeStart.store(true, std::memory_order_release);
    }
}

void MediaGraphExecutionContext::cancelSessionPayloadWaiters() noexcept
{
    // Segment failure must never revoke another failure domain's admission.
    // Session termination withdraws only pending/granted-but-unclaimed demands;
    // live payload leases remain charged until their last actual reference dies.
    if (m_payloadCreditLedger && m_ownsPayloadLedger)
        m_payloadCreditLedger->cancelBlockedWaiters();
}

void MediaGraphExecutionContext::reset()
{
    shutdownAvSyncGroups();
    m_graph = nullptr;
    m_graphOwner.reset();
    m_channels.clear();
    cancelSessionPayloadWaiters();
    m_ownsPayloadLedger = true;
    m_payloadCreditLedger.reset();
    m_inputActivity.reset();
    m_executionOrder.clear();
    m_nodeWakeups.clear();
    m_nodeExitTokens.clear();
    m_compiled = false;
    if (m_ownsGlobalDiagnostics) mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

std::shared_ptr<MediaGraphPayloadCreditLedger>
MediaGraphExecutionContext::payloadCreditLedger() const noexcept
{
    return m_payloadCreditLedger;
}

bool MediaGraphExecutionContext::payloadCreditsRequired() const noexcept
{
    return m_graph && m_graph->payloadCreditMode() &&
           *m_graph->payloadCreditMode() ==
               MediaGraphPayloadCreditMode::RealtimeRequired;
}

std::shared_ptr<MediaInputActivity>
MediaGraphExecutionContext::inputActivity() const noexcept
{
    return m_inputActivity;
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
        if (!payloadCreditsRequired()) {
            std::vector<MediaGraphPayloadReservation> reservations;
            const std::size_t count = actualBytes.empty() ? 1 : actualBytes.size();
            reservations.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                reservations.push_back(
                    MediaGraphPayloadReservation::nonRealtimeNotApplicable());
            }
            return Result::success(std::move(reservations));
        }
        return Result::failure(::media::ErrorInfo::notInitialized(
            "runtime graph has no activated payload credit ledger"));
    }
    const auto& strategies = m_graph->payloadCreditPlan()->producers;
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
    if (selected->payloadKind == MediaPayloadKind::Frame &&
        !selected->frameCredit) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "runtime frame producer lacks its typed credit contract"));
    }
    const bool accountsBytes = selected->payloadKind == MediaPayloadKind::Frame
        ? selected->frameCredit->allocationScope ==
            MediaFrameCreditAllocationScope::EngineLogicalBytes
        : selected->accounting ==
            MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject;
    const auto expectedAccounting = accountsBytes
        ? MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject
        : MediaGraphPayloadAllocationAccounting::
            ObservedOnlyExternalBytesAndEngineManagedObject;
    if (selected->accounting != expectedAccounting) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "runtime payload producer accounting conflicts with its typed frame contract"));
    }
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
    auto leases = m_payloadCreditLedger->tryReserveOrArm(
        producer, ledgerBytes, sharedNodeWakeup(producer));
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
    if (m_ownsGlobalDiagnostics) mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
}

bool MediaGraphExecutionContext::diagnosticsEnabled() const noexcept
{
    return m_diagnosticConfig.level != MediaGraphDiagnosticLevel::Off;
}

void MediaGraphExecutionContext::setDiagnosticConfig(MediaGraphDiagnosticConfig config) noexcept
{
    m_diagnosticConfig = config;
    if (m_ownsGlobalDiagnostics) mediaGraphDiagnosticSetGlobalConfig(m_diagnosticConfig);
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

std::shared_ptr<MediaGraphWorkerExitToken> MediaGraphExecutionContext::nodeExitToken(
    MediaNodeId nodeId) const noexcept
{
    const auto found = m_nodeExitTokens.find(nodeId.value);
    return found == m_nodeExitTokens.end() ? nullptr : found->second;
}

std::shared_ptr<MediaNodeWakeup> MediaGraphExecutionContext::findNodeWakeup(
    MediaNodeId nodeId) const noexcept
{
    const auto found = m_nodeWakeups.find(nodeId.value);
    return found == m_nodeWakeups.end() ? nullptr : found->second;
}

MediaNodeWakeup& MediaGraphExecutionContext::nodeWakeup(MediaNodeId nodeId)
{
    return *sharedNodeWakeup(nodeId);
}

std::shared_ptr<MediaNodeWakeup>
MediaGraphExecutionContext::sharedNodeWakeup(MediaNodeId nodeId)
{
    const auto existing = m_nodeWakeups.find(nodeId.value);
    if (existing != m_nodeWakeups.end()) return existing->second;
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
            channel->setConsumerWakeup(sharedNodeWakeup(edge.to.nodeId));
            channel->setProducerWakeup(sharedNodeWakeup(edge.from.nodeId));
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
    for (const auto id : m_executionOrder)
        m_nodeExitTokens.emplace(id.value, std::make_shared<MediaGraphWorkerExitToken>());
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
