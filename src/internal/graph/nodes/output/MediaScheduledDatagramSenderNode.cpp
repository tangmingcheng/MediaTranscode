#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <limits>
#include <new>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

MediaScheduledDatagramSenderNode::MediaScheduledDatagramSenderNode(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    MediaScheduledDatagramSenderNodeDependencies dependencies) noexcept
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaScheduledDatagramSenderNode"),
      m_plannedSession(std::move(plannedSession)),
      m_streamSet(streamSet),
      m_clock(std::move(dependencies.clock)),
      m_portFactory(std::move(dependencies.portFactory))
{
}

MediaScheduledDatagramSenderNode::~MediaScheduledDatagramSenderNode() = default;

::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>
MediaScheduledDatagramSenderNode::create(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    MediaScheduledDatagramSenderNodeDependencies dependencies)
{
    using Result = ::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>;
    if (!nodeId.isValid() || !plannedSession.valid() || !dependencies.clock ||
        !dependencies.portFactory ||
        dependencies.clock->sessionKey() != plannedSession ||
        dependencies.clock->streamSet() != streamSet) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires exact clock and transport dependencies"));
    }
    auto node = std::unique_ptr<MediaScheduledDatagramSenderNode>(
        new (std::nothrow) MediaScheduledDatagramSenderNode(
            nodeId, std::move(plannedSession), streamSet,
            std::move(dependencies)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledDatagramSenderNode"));
    }
    return Result::success(std::move(node));
}

MediaNodeKind MediaScheduledDatagramSenderNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledDatagramSender;
}

::media::Status MediaScheduledDatagramSenderNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto* plan = context.findInputChannel(nodeId(), "plan");
    const auto* batch = context.findInputChannel(nodeId(), "batch");
    const auto* graph = context.graph();
    const auto* planPort = graph ? graph->findInputPort(nodeId(), "plan") : nullptr;
    const auto* batchPort = graph ? graph->findInputPort(nodeId(), "batch") : nullptr;
    std::size_t planChannels = 0;
    std::size_t batchChannels = 0;
    bool channelTypesValid = true;
    for (const auto* channel : context.inputChannels(nodeId())) {
        if (!channel) {
            channelTypesValid = false;
            continue;
        }
        if (planPort && channel->binding().to.portId == planPort->id) {
            ++planChannels;
            channelTypesValid = channelTypesValid &&
                channel->binding().streamKind == MediaStreamKind::Metadata &&
                channel->binding().payloadKind ==
                    MediaPayloadKind::DatagramTransportPlan;
        } else if (batchPort &&
                   channel->binding().to.portId == batchPort->id) {
            ++batchChannels;
            channelTypesValid = channelTypesValid &&
                channel->binding().streamKind == MediaStreamKind::Metadata &&
                channel->binding().payloadKind ==
                    MediaPayloadKind::WireDatagramBatch;
        } else {
            channelTypesValid = false;
        }
    }
    if (!context.outputChannels(nodeId()).empty() || !plan || !batch ||
        !planPort || !batchPort || planChannels != 1 || batchChannels == 0 ||
        !channelTypesValid ||
        plan->binding().streamKind != MediaStreamKind::Metadata ||
        plan->binding().payloadKind != MediaPayloadKind::DatagramTransportPlan ||
        batch->binding().streamKind != MediaStreamKind::Metadata ||
        batch->binding().payloadKind != MediaPayloadKind::WireDatagramBatch) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires exact transport plan and scheduled wire inputs"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::start(
    MediaGraphExecutionContext& context)
{
    m_session.reset();
    m_pacingController.reset();
    m_serviceLedger.reset();
    m_pendingBatch.reset();
    m_generation.reset();
    m_serviceScopeId.clear();
    m_executionMode = MediaDatagramTransmitExecutionMode::UserspaceNonblocking;
    m_wireOverheadBytes.clear();
    m_endpointDatagrams.clear();
    m_endpointBytes.clear();
    m_endpointIds.clear();
    m_submitEntries.clear();
    m_queuedWireBatches.clear();
    m_burstWireBytes = 0;
    m_maximumBatchDatagrams = 0;
    m_maximumBatchBytes = 0;
    m_maximumBacklogDatagrams = 0;
    m_maximumBacklogBytes = 0;
    m_queuedWireDatagrams = 0;
    m_queuedWireBytes = 0;
    m_nextScheduledSequence.reset();
    m_state = SubmitState::WaitReservation;
    m_nextDatagram = 0;
    m_groupBegin = 0;
    m_groupCount = 0;
    m_groupEndpointId = 0;
    m_groupNotBefore = MediaRunningTime::fromNanoseconds(0);
    m_groupDeadline = MediaRunningTime::fromNanoseconds(0);
    m_groupDeadlineSubmitAttempted = false;
    m_lastSubmittedAt.reset();
    m_terminalFailure.reset();
    m_wakeup.reset();
    m_stopSource = std::stop_source{};
    m_batches = 0;
    m_datagrams = 0;
    m_bytes = 0;
    m_wouldBlockEvents = 0;
    m_writableWaits = 0;
    m_deadlineMisses = 0;
    m_pressureFailures = 0;
    m_partialSubmittedFailures = 0;
    m_ambiguousSubmittedFailures = 0;
    m_commitAttempted = false;
    m_diagnosticsEmitted = false;
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Status MediaScheduledDatagramSenderNode::bindPlan(
    const MediaDatagramTransportPlanBuffer& planBuffer)
{
    const auto& plan = planBuffer.plan();
    auto activation = m_clock->currentActivation();
    if (!activation || !planBuffer.globalSequence() ||
        plan.shaping.sessionKey() != m_plannedSession.value() ||
        activation.value().generation != plan.shaping.generation() ||
        (m_generation && plan.shaping.generation() <= *m_generation) ||
        plan.shaping.serviceScope().scopeId.empty() ||
        plan.localEndpoints.size() != plan.shaping.endpoints().size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender plan differs from active session or generation"));
    }

    std::vector<MediaDatagramTransmitEndpointBinding> bindings;
    bindings.reserve(plan.localEndpoints.size());
    for (const auto& local : plan.localEndpoints) {
        if (!plan.shaping.endpoint(local.endpointId)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram sender local endpoint is outside service scope"));
        }
        auto endpoint = MediaUdpDatagramEndpoint::create(
            local.addressFamily, local.numericAddress, local.port);
        if (!endpoint) return ::media::Status::failure(endpoint.error());
        bindings.push_back(MediaDatagramTransmitEndpointBinding{
            local.endpointId, std::move(endpoint).value()});
    }

    MediaDatagramTransmitExecutionMode mode;
    switch (plan.execution) {
    case MediaDatagramTransportExecutionKind::UserspaceNonblocking:
        mode = MediaDatagramTransmitExecutionMode::UserspaceNonblocking;
        break;
    default:
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender execution mode is unknown"));
    }
    MediaDatagramTransmitExecutionPlan execution{
        mode, plan.executionAuthority, std::nullopt};
    auto shaping = plan.shaping.clone();
    if (!shaping) return ::media::Status::failure(shaping.error());
    if (auto valid = MediaDatagramTransmitSession::validateActivation(
            shaping.value(), bindings, execution); !valid) {
        return valid;
    }
    if (!m_queuedWireBatches.empty() || m_pendingBatch) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender cannot rebind with queued wire work"));
    }
    if (m_session) {
        auto now = m_clock->now();
        if (!now) return ::media::Status::failure(now.error());
        auto closed = m_session->close(now.value());
        if (!closed) return closed;
        m_session.reset();
    }
    auto session = MediaDatagramTransmitSession::create(
        shaping.value(), std::move(bindings), std::move(execution),
        *m_portFactory);
    if (!session) return ::media::Status::failure(session.error());
    MediaDatagramPacingContract pacingContract{
        plan.shaping.sessionKey(), plan.shaping.serviceScope().scopeId,
        plan.shaping.generation(),
        plan.shaping.serviceCurve().pacingWireBytesPerSecond};
    if (m_pacingController) {
        auto rebound = m_pacingController->rebind(std::move(pacingContract));
        if (!rebound) return rebound;
    } else {
        auto pacing = MediaDatagramPacingController::create(
            std::move(pacingContract));
        if (!pacing) return ::media::Status::failure(pacing.error());
        m_pacingController = std::move(pacing).value();
    }
    m_session = std::move(session).value();
    m_serviceLedger = planBuffer.globalSequence();
    m_generation = plan.shaping.generation();
    m_serviceScopeId = plan.shaping.serviceScope().scopeId;
    m_executionMode = mode;
    m_wireOverheadBytes.clear();
    m_endpointIds.clear();
    m_endpointDatagrams.clear();
    m_endpointBytes.clear();
    for (const auto& endpoint : plan.shaping.endpoints()) {
        m_endpointIds.push_back(endpoint.endpointId);
        m_endpointDatagrams.emplace(endpoint.endpointId, 0);
        m_endpointBytes.emplace(endpoint.endpointId, 0);
        m_wireOverheadBytes.emplace(
            endpoint.endpointId,
            endpoint.mtuEvidence.ipHeaderBytes +
                endpoint.mtuEvidence.transportHeaderBytes);
    }
    m_burstWireBytes = plan.shaping.serviceCurve().burstWireBytes;
    m_maximumBatchDatagrams = plan.shaping.batch().maximumDatagrams;
    m_maximumBatchBytes = plan.shaping.batch().maximumBytes;
    m_maximumBacklogDatagrams = plan.shaping.backlog().maximumDatagrams;
    m_maximumBacklogBytes = plan.shaping.backlog().maximumBytes;
    const auto snapshot = m_serviceLedger->snapshot();
    m_nextScheduledSequence = snapshot.nextGlobalSequence;
    if (m_maximumBatchDatagrams > m_submitEntries.max_size() ||
        m_maximumBacklogDatagrams > m_queuedWireBatches.max_size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender batch capacity is not representable"));
    }
    try {
        m_submitEntries.reserve(
            static_cast<std::size_t>(m_maximumBatchDatagrams));
        m_queuedWireBatches.reserve(
            static_cast<std::size_t>(m_maximumBacklogDatagrams));
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "scheduled datagram sender submit storage"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::waitUntil(
    MediaRunningTime deadline)
{
    while (true) {
        auto now = m_clock->now();
        if (!now) return ::media::Status::failure(now.error());
        if (now.value() >= deadline) return ::media::Status::success();
        const auto remaining = deadline.nanoseconds() - now.value().nanoseconds();
        const auto sequence = m_wakeup.sequence();
        auto waited = m_wakeup.wait(
            sequence, MediaNodeDeadlineWakePolicy::DeadlineOrCancellation,
            std::chrono::nanoseconds(remaining));
        if (!waited) return ::media::Status::failure(waited.error());
        if (waited.value() == MediaNodeWakeup::WaitOutcome::Interrupted) {
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                "scheduled datagram sender reservation wait was interrupted"));
        }
    }
}

::media::Status MediaScheduledDatagramSenderNode::beginSubmitGroup(
    MediaRunningTime now)
{
    if (!m_pendingBatch || !m_generation || !m_session ||
        !m_pacingController ||
        m_nextDatagram >= m_pendingBatch->m_datagrams.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender has no reservable wire job"));
    }
    auto& first = m_pendingBatch->m_datagrams[m_nextDatagram];
    if (first.generation() != *m_generation ||
        !m_pendingBatch->m_commitSlice.valid()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled wire job violates generation or commit ownership"));
    }
    m_groupBegin = m_nextDatagram;
    const auto overhead = m_wireOverheadBytes.find(first.endpointId());
    if (overhead == m_wireOverheadBytes.end() ||
        first.bytes().size() > m_maximumBatchBytes ||
        first.bytes().size() + overhead->second > m_burstWireBytes) {
        ++m_pressureFailures;
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled wire job exceeds the activated batch or burst envelope"));
    }
    m_groupCount = 1;
    m_groupEndpointId = first.endpointId();
    auto pacing = m_pacingController->reserve(
        MediaDatagramPacingJob{
            first.generation(), first.endpointId(), first.globalSequence(),
            static_cast<std::uint64_t>(first.bytes().size()) + overhead->second,
            first.canonicalRelease(), first.canonicalDeadline()},
        now);
    if (!pacing) return ::media::Status::failure(pacing.error());
    m_groupNotBefore = pacing.value().notBefore;
    m_groupDeadline = pacing.value().notAfter;
    if (m_groupNotBefore > m_groupDeadline) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "scheduled wire job cannot satisfy its GBRA completion deadline"));
    }
    m_groupDeadlineSubmitAttempted = false;
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::preflightBatchTelemetry(
    const MediaWireDatagramBatchBuffer& batch) const
{
    if (batch.m_datagrams.empty() ||
        batch.m_datagrams.size() > m_maximumBatchDatagrams ||
        m_batches == (std::numeric_limits<std::uint64_t>::max)() ||
        batch.m_datagrams.size() >
            (std::numeric_limits<std::uint64_t>::max)() - m_datagrams) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "scheduled datagram sender batch telemetry overflowed"));
    }
    std::uint64_t batchBytes = 0;
    for (const auto& datagram : batch.m_datagrams) {
        if (m_endpointDatagrams.find(datagram.endpointId()) ==
                m_endpointDatagrams.end() ||
            m_endpointBytes.find(datagram.endpointId()) ==
                m_endpointBytes.end() ||
            datagram.bytes().size() >
                (std::numeric_limits<std::uint64_t>::max)() - batchBytes) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "scheduled datagram sender batch endpoint or byte telemetry is invalid"));
        }
        batchBytes += static_cast<std::uint64_t>(datagram.bytes().size());
    }
    if (batchBytes > (std::numeric_limits<std::uint64_t>::max)() - m_bytes) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "scheduled datagram sender batch byte telemetry overflowed"));
    }
    for (const auto endpointId : m_endpointIds) {
        std::uint64_t endpointDatagramCount = 0;
        std::uint64_t endpointByteCount = 0;
        for (const auto& datagram : batch.m_datagrams) {
            if (datagram.endpointId() != endpointId) continue;
            if (endpointDatagramCount ==
                    (std::numeric_limits<std::uint64_t>::max)() ||
                datagram.bytes().size() >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        endpointByteCount) {
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "scheduled datagram sender endpoint batch telemetry overflowed"));
            }
            ++endpointDatagramCount;
            endpointByteCount +=
                static_cast<std::uint64_t>(datagram.bytes().size());
        }
        const auto datagrams = m_endpointDatagrams.find(endpointId);
        const auto bytes = m_endpointBytes.find(endpointId);
        if (datagrams == m_endpointDatagrams.end() ||
            bytes == m_endpointBytes.end() ||
            endpointDatagramCount >
                (std::numeric_limits<std::uint64_t>::max)() - datagrams->second ||
            endpointByteCount >
                (std::numeric_limits<std::uint64_t>::max)() - bytes->second) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "scheduled datagram sender endpoint cumulative telemetry overflowed"));
        }
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::enqueueWireBatch(
    std::shared_ptr<MediaWireDatagramBatchBuffer> batch)
{
    if (!batch || batch->m_datagrams.empty() || !m_nextScheduledSequence ||
        batch->m_datagrams.size() > m_maximumBacklogDatagrams) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "common pacing sender requires one ordered bounded wire batch"));
    }
    const auto first = batch->m_datagrams.front().globalSequence();
    const auto last = batch->m_datagrams.back().globalSequence();
    if (first < *m_nextScheduledSequence || last < first) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "common pacing sender rejects stale or wrapped wire sequence"));
    }

    std::uint64_t wireBytes = 0;
    for (const auto& datagram : batch->m_datagrams) {
        const auto overhead = m_wireOverheadBytes.find(datagram.endpointId());
        const auto payloadBytes =
            static_cast<std::uint64_t>(datagram.bytes().size());
        if (overhead == m_wireOverheadBytes.end() ||
            payloadBytes > (std::numeric_limits<std::uint64_t>::max)() -
                               overhead->second ||
            payloadBytes + overhead->second >
                (std::numeric_limits<std::uint64_t>::max)() - wireBytes) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "common pacing sender wire cost is not representable"));
        }
        wireBytes += payloadBytes + overhead->second;
    }
    const auto datagrams =
        static_cast<std::uint64_t>(batch->m_datagrams.size());
    if (datagrams > m_maximumBacklogDatagrams - m_queuedWireDatagrams ||
        wireBytes > m_maximumBacklogBytes - m_queuedWireBytes) {
        ++m_pressureFailures;
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "common pacing sender ordered queue exceeds planner backlog"));
    }

    auto position = std::lower_bound(
        m_queuedWireBatches.begin(), m_queuedWireBatches.end(), first,
        [](const QueuedWireBatch& queued, std::uint64_t sequence) {
            return queued.firstGlobalSequence < sequence;
        });
    if ((position != m_queuedWireBatches.end() &&
         last >= position->firstGlobalSequence) ||
        (position != m_queuedWireBatches.begin() &&
         std::prev(position)->lastGlobalSequence >= first)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "common pacing sender rejects overlapping wire sequence ranges"));
    }
    try {
        m_queuedWireBatches.insert(
            position, QueuedWireBatch{first, last, wireBytes, std::move(batch)});
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "common pacing sender ordered wire queue"));
    }
    m_queuedWireDatagrams += datagrams;
    m_queuedWireBytes += wireBytes;
    m_maximumQueuedWireBatches = (std::max)(
        m_maximumQueuedWireBatches,
        static_cast<std::uint64_t>(m_queuedWireBatches.size()));
    m_maximumQueuedWireDatagrams = (std::max)(
        m_maximumQueuedWireDatagrams, m_queuedWireDatagrams);
    m_maximumQueuedWireBytes = (std::max)(
        m_maximumQueuedWireBytes, m_queuedWireBytes);
    return ::media::Status::success();
}

::media::Result<bool>
MediaScheduledDatagramSenderNode::activateNextWireBatch()
{
    using Result = ::media::Result<bool>;
    if (m_pendingBatch || m_queuedWireBatches.empty()) {
        return Result::success(false);
    }
    if (!m_nextScheduledSequence ||
        m_queuedWireBatches.front().firstGlobalSequence !=
            *m_nextScheduledSequence) {
        return Result::success(false);
    }
    auto& queued = m_queuedWireBatches.front();
    if (queued.lastGlobalSequence ==
        (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "common pacing sender next sequence would overflow"));
    }
    auto scheduledAt = m_clock->now();
    if (!scheduledAt) return Result::failure(scheduledAt.error());
    auto scheduled = queued.batch->m_commitSlice.scheduleAll(
        scheduledAt.value());
    if (!scheduled) return Result::failure(scheduled.error());
    const auto datagrams = static_cast<std::uint64_t>(
        queued.batch->m_datagrams.size());
    if (datagrams > m_queuedWireDatagrams ||
        queued.wireBytes > m_queuedWireBytes) {
        return Result::failure(::media::ErrorInfo::internalError(
            "common pacing sender ordered queue accounting underflowed"));
    }
    m_pendingBatch = std::move(queued.batch);
    *m_nextScheduledSequence = queued.lastGlobalSequence + 1;
    m_queuedWireDatagrams -= datagrams;
    m_queuedWireBytes -= queued.wireBytes;
    m_queuedWireBatches.erase(m_queuedWireBatches.begin());
    m_nextDatagram = 0;
    m_lastSubmittedAt.reset();
    m_commitAttempted = false;
    m_state = SubmitState::WaitReservation;
    return Result::success(true);
}

bool MediaScheduledDatagramSenderNode::allBatchInputsDrained(
    MediaGraphExecutionContext& context) const noexcept
{
    const auto* graph = context.graph();
    const auto* batchPort = graph
        ? graph->findInputPort(nodeId(), "batch")
        : nullptr;
    if (!batchPort) return false;
    bool found = false;
    for (const auto* channel : context.inputChannels(nodeId())) {
        if (!channel || channel->binding().to.portId != batchPort->id) {
            continue;
        }
        found = true;
        if (!channel->closed() || channel->size() != 0) return false;
    }
    return found;
}

void MediaScheduledDatagramSenderNode::recordSubmittedPrefix(
    std::size_t count,
    MediaRunningTime submitCompletedAt) noexcept
{
    m_lastSubmittedAt = submitCompletedAt;
    m_nextDatagram += count;
}

::media::Status
MediaScheduledDatagramSenderNode::commitAccumulatedSubmittedPrefix()
{
    if (!m_pendingBatch || m_commitAttempted || !m_lastSubmittedAt ||
        m_nextDatagram == 0 ||
        m_nextDatagram > m_pendingBatch->m_datagrams.size()) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "scheduled datagram sender has no valid accumulated submitted prefix"));
    }
    m_commitAttempted = true;
    auto committed = m_pendingBatch->m_commitSlice.commitSubmittedPrefix(
        m_nextDatagram, *m_lastSubmittedAt);
    if (!committed) return committed;

    std::uint64_t submittedBytes = 0;
    for (std::size_t index = 0; index < m_nextDatagram; ++index) {
        const auto& datagram = m_pendingBatch->m_datagrams[index];
        const auto bytes = static_cast<std::uint64_t>(datagram.bytes().size());
        submittedBytes += bytes;
        auto endpointDatagrams = m_endpointDatagrams.find(datagram.endpointId());
        auto endpointBytes = m_endpointBytes.find(datagram.endpointId());
        ++endpointDatagrams->second;
        endpointBytes->second += bytes;
    }
    m_datagrams += static_cast<std::uint64_t>(m_nextDatagram);
    m_bytes += submittedBytes;
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::failSubmit(
    const MediaDatagramTransmitError& error,
    MediaRunningTime submitStartedAt,
    MediaRunningTime submitCompletedAt)
{
    if (error.kind ==
        MediaDatagramTransmitFailureKind::PartialSubmittedPrefix) {
        ++m_partialSubmittedFailures;
    } else if (error.kind ==
               MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix) {
        ++m_ambiguousSubmittedFailures;
    }
    if ((error.kind ==
             MediaDatagramTransmitFailureKind::PartialSubmittedPrefix ||
         error.kind ==
             MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix) &&
        error.submittedPrefixDatagrams != 0) {
        if (error.kind ==
            MediaDatagramTransmitFailureKind::PartialSubmittedPrefix) {
            const auto submittedSequence =
                m_pendingBatch->m_datagrams[m_groupBegin].globalSequence();
            auto paced = m_pacingController->markSubmitted(
                submittedSequence, submitStartedAt, submitCompletedAt);
            if (!paced) return failTerminal(paced.error());
        }
        recordSubmittedPrefix(
            static_cast<std::size_t>(error.submittedPrefixDatagrams),
            submitCompletedAt);
    }
    return failTerminal(error.cause);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::progressPendingBatch()
{
    while (m_pendingBatch) {
        if (m_nextDatagram == m_pendingBatch->m_datagrams.size()) {
            auto committed = commitAccumulatedSubmittedPrefix();
            if (!committed) return failTerminal(committed.error());
            ++m_batches;
            m_pendingBatch.reset();
            m_nextDatagram = 0;
            m_commitAttempted = false;
            m_state = SubmitState::WaitReservation;
            return processProgress();
        }

        if (m_state == SubmitState::WaitReservation) {
            auto reservationTime = m_clock->now();
            if (!reservationTime) return failTerminal(reservationTime.error());
            auto begun = beginSubmitGroup(reservationTime.value());
            if (!begun) return failTerminal(begun.error());
            auto waited = waitUntil(m_groupNotBefore);
            if (!waited) return failTerminal(waited.error());
            m_state = SubmitState::TrySubmit;
        }

        if (m_state == SubmitState::WaitWritableWithinOriginalDeadline) {
            auto now = m_clock->now();
            if (!now) return failTerminal(now.error());
            if (now.value() > m_groupDeadline) {
                ++m_deadlineMisses;
                return failTerminal(::media::ErrorInfo::ioFailure(
                    "scheduled datagram remained blocked through its original deadline"));
            }
            if (now.value() == m_groupDeadline) {
                m_state = SubmitState::TrySubmit;
                continue;
            }
            auto remaining = m_groupDeadline.checkedSubtract(now.value());
            if (!remaining) return failTerminal(remaining.error());
            ++m_writableWaits;
            auto waited = m_session->waitWritable(
                m_groupEndpointId, now.value(), remaining.value(),
                m_stopSource.get_token());
            if (!waited) return failTerminal(waited.error());
            if (waited.value() == MediaDatagramWritableWaitResult::TimedOut) {
                ++m_deadlineMisses;
                return failTerminal(::media::ErrorInfo::ioFailure(
                    "scheduled datagram writability wait reached its original deadline"));
            }
            if (waited.value() == MediaDatagramWritableWaitResult::Stopped) {
                return failTerminal(::media::ErrorInfo::cancelled(
                    "scheduled datagram writability wait was stopped"));
            }
            m_state = SubmitState::TrySubmit;
        }

        if (m_state == SubmitState::TrySubmit) {
            auto now = m_clock->now();
            if (!now) return failTerminal(now.error());
            if (now.value() < m_groupNotBefore) {
                return failTerminal(::media::ErrorInfo::internalError(
                    "scheduled datagram submit violated physical service spacing"));
            }
            if (now.value() > m_groupDeadline) {
                ++m_deadlineMisses;
                return failTerminal(::media::ErrorInfo::ioFailure(
                    "scheduled datagram submit exceeded its original deadline"));
            }
            if (now.value() == m_groupDeadline) {
                if (m_groupDeadlineSubmitAttempted) {
                    ++m_deadlineMisses;
                    return failTerminal(::media::ErrorInfo::ioFailure(
                        "scheduled datagram remained blocked at its original deadline"));
                }
                m_groupDeadlineSubmitAttempted = true;
            }
            MediaDatagramTransmitSubmitResult submitted =
                MediaDatagramTransmitSubmitResult::failure(
                    mediaDatagramTransmitError(::media::ErrorInfo::internalError(
                        "scheduled datagram sender did not issue a submit")));
            if (m_session->hasPendingRetry()) {
                submitted = m_session->retryPending(now.value());
            } else {
                m_submitEntries.clear();
                for (std::size_t offset = 0; offset < m_groupCount; ++offset) {
                    const auto& datagram =
                        m_pendingBatch->m_datagrams[m_groupBegin + offset];
                    m_submitEntries.push_back(MediaDatagramTransmitJobEntry{
                        datagram.bytes(), datagram.globalSequence(),
                        m_groupDeadline, std::nullopt});
                }
                submitted = m_session->trySubmitNew(
                    m_groupEndpointId, m_submitEntries, now.value());
            }
            if (!submitted) {
                auto submitCompletedAt = m_clock->now();
                if (!submitCompletedAt) {
                    return failTerminal(submitCompletedAt.error());
                }
                return failSubmit(
                    submitted.error(), now.value(), submitCompletedAt.value());
            }
            if (submitted.value() == MediaDatagramTransmitAttempt::WouldBlock) {
                ++m_wouldBlockEvents;
                m_state = SubmitState::WaitWritableWithinOriginalDeadline;
                continue;
            }
            if (submitted.value() != MediaDatagramTransmitAttempt::Submitted) {
                return failTerminal(::media::ErrorInfo::internalError(
                    "scheduled datagram transport returned an unknown submit outcome"));
            }
            const auto submittedSequence =
                m_pendingBatch->m_datagrams[m_groupBegin].globalSequence();
            auto submitCompletedAt = m_clock->now();
            if (!submitCompletedAt) {
                return failTerminal(submitCompletedAt.error());
            }
            auto paced = m_pacingController->markSubmitted(
                submittedSequence, now.value(), submitCompletedAt.value());
            if (!paced) return failTerminal(paced.error());
            recordSubmittedPrefix(m_groupCount, submitCompletedAt.value());
            m_state = SubmitState::WaitReservation;
        }
    }
    return processProgress();
}

void MediaScheduledDatagramSenderNode::emitDiagnostics(
    const char* stage) noexcept
{
    if (m_diagnosticsEmitted) return;
    m_diagnosticsEmitted = true;
    try {
        std::ostringstream diagnostic;
        diagnostic << "scheduled_datagram_sender stage=" << stage
                   << " generation=" << m_generation.value_or(0)
                   << " service_scope=" << m_serviceScopeId
                   << " committed_batches=" << m_batches
                   << " committed_datagrams=" << m_datagrams
                   << " committed_payload_bytes=" << m_bytes
                   << " would_block=" << m_wouldBlockEvents
                   << " writable_waits=" << m_writableWaits
                   << " deadline_misses=" << m_deadlineMisses
                   << " pressure_failures=" << m_pressureFailures
                   << " partial_submitted_failures="
                   << m_partialSubmittedFailures
                   << " ambiguous_submitted_failures="
                   << m_ambiguousSubmittedFailures
                   << " ordered_queue_maximum_batches="
                   << m_maximumQueuedWireBatches
                   << " ordered_queue_maximum_datagrams="
                   << m_maximumQueuedWireDatagrams
                   << " ordered_queue_maximum_wire_bytes="
                   << m_maximumQueuedWireBytes
                   << " pacing_reserved="
                   << (m_pacingController
                           ? m_pacingController->telemetry().reservedDatagrams
                           : 0)
                   << " pacing_submitted="
                   << (m_pacingController
                           ? m_pacingController->telemetry().submittedDatagrams
                           : 0)
                   << " pacing_maximum_submit_lateness_ns="
                   << (m_pacingController
                           ? m_pacingController->telemetry()
                                 .maximumSubmitLatenessNanoseconds
                           : 0)
                   << " delivery_evidence=not_proven";
        if (m_serviceLedger) {
            const auto backlog = m_serviceLedger->snapshot();
            diagnostic
                << " backlog_current_datagrams=" << backlog.currentDatagrams
                << " backlog_current_wire_bytes=" << backlog.currentWireBytes
                << " backlog_high_water_datagrams=" << backlog.highWaterDatagrams
                << " backlog_high_water_wire_bytes=" << backlog.highWaterWireBytes
                << " backlog_max_residence_ns="
                << backlog.maximumResidenceNanoseconds
                << " last_materialized_sequence="
                << backlog.lastMaterializedSequence.value_or(0)
                << " last_scheduled_sequence="
                << backlog.lastScheduledSequence.value_or(0)
                << " last_submitted_sequence="
                << backlog.lastSubmittedSequence.value_or(0)
                << " last_committed_sequence="
                << backlog.lastCommittedSequence.value_or(0);
        }
        if (m_session) {
            const auto& evidence = m_session->evidenceTelemetry();
            diagnostic << " evidence_submitted=" << evidence.submitted
                       << " evidence_timestamp_tracked="
                       << evidence.timestampTracked
                       << " evidence_timestamp_untracked="
                       << evidence.timestampUntracked
                       << " evidence_observed=" << evidence.observed
                       << " evidence_lost=" << evidence.lost
                       << " evidence_late=" << evidence.late
                       << " evidence_duplicate=" << evidence.duplicate
                       << " evidence_unmatched=" << evidence.unmatched
                       << " evidence_coverage_complete="
                       << (evidence.transmitTimestampCoverageComplete ? 1 : 0)
                       << " aggregate_effective_socket_bytes="
                       << m_session->effectiveSocketBytes();
            for (const auto endpointId : m_endpointIds) {
                const auto* capabilities = m_session->capabilities(endpointId);
                if (!capabilities) continue;
                diagnostic
                    << " endpoint_" << endpointId << "_target_effective_socket_bytes="
                    << capabilities->targetEffectiveSendBufferBytes
                    << " endpoint_" << endpointId << "_api_requested_socket_bytes="
                    << capabilities->apiRequestedSendBufferBytes
                    << " endpoint_" << endpointId << "_effective_socket_bytes="
                    << capabilities->effectiveSendBufferBytes
                    << " endpoint_" << endpointId << "_timestamp_source="
                    << static_cast<int>(capabilities->timestampSource)
                    << " endpoint_" << endpointId << "_committed_datagrams="
                    << m_endpointDatagrams[endpointId]
                    << " endpoint_" << endpointId << "_committed_payload_bytes="
                    << m_endpointBytes[endpointId];
            }
        }
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            diagnostic.str());
    } catch (...) {
    }
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::failTerminal(::media::ErrorInfo error)
{
    if (!m_terminalFailure) {
        if (m_pendingBatch && m_nextDatagram != 0 && !m_commitAttempted) {
            auto committed = commitAccumulatedSubmittedPrefix();
            if (!committed) error = committed.error();
        }
        m_terminalFailure = std::move(error);
        if (m_session && m_clock) {
            auto now = m_clock->now();
            if (now) m_session->abort(*m_terminalFailure, now.value());
        }
    }
    emitDiagnostics("failed");
    return ::media::Result<MediaNodeProcessResult>::failure(*m_terminalFailure);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return failTerminal(*m_terminalFailure);
    if (m_pendingBatch) return progressPendingBatch();

    auto planInput = tryPopInputOptional(context, "plan");
    if (!planInput) return failTerminal(planInput.error());
    if (planInput.value()) {
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
                planInput.value()->get())) {
            if (control->controlKind() == MediaControlBufferKind::Abort) {
                return failTerminal(::media::ErrorInfo::cancelled(
                    "scheduled datagram sender plan was aborted"));
            }
        } else {
            const auto* plan = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
                planInput.value()->get());
            if (!plan) return failTerminal(::media::ErrorInfo::invalidArgument(
                "scheduled datagram sender requires an activated transport plan"));
            auto bound = bindPlan(*plan);
            if (!bound) return failTerminal(bound.error());
        }
    }
    if (!m_session) {
        const auto* channel = context.findInputChannel(nodeId(), "plan");
        if (channel && channel->closed()) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "scheduled datagram sender plan input closed before activation"));
        }
        return processWaiting();
    }

    auto activated = activateNextWireBatch();
    if (!activated) return failTerminal(activated.error());
    if (activated.value()) return progressPendingBatch();

    static constexpr std::array<std::string_view, 1> BatchPortNames{"batch"};
    auto batchInput = tryPopFirstInputWithChannelOptional(
        context, BatchPortNames);
    if (!batchInput) return failTerminal(batchInput.error());
    if (!batchInput.value()) {
        if (!allBatchInputsDrained(context)) return processWaiting();
        if (!m_queuedWireBatches.empty()) {
            return failTerminal(::media::ErrorInfo::internalError(
                "common pacing sender inputs closed with a global sequence gap"));
        }
        return processFinished();
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            batchInput.value()->buffer.get())) {
        if (control->controlKind() == MediaControlBufferKind::Abort) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "scheduled datagram sender received abort"));
        }
        return processProgress();
    }
    auto batch = std::dynamic_pointer_cast<MediaWireDatagramBatchBuffer>(
        batchInput.value()->buffer);
    if (!batch || batch->sessionKey() != m_plannedSession.value() ||
        batch->serviceScopeId() != m_serviceScopeId || !m_generation ||
        batch->generation() != *m_generation || batch->m_datagrams.empty()) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender received a batch outside the active service scope"));
    }
    auto telemetry = preflightBatchTelemetry(*batch);
    if (!telemetry) return failTerminal(telemetry.error());
    auto queued = enqueueWireBatch(std::move(batch));
    if (!queued) return failTerminal(queued.error());
    activated = activateNextWireBatch();
    if (!activated) return failTerminal(activated.error());
    return activated.value() ? progressPendingBatch() : processProgress();
}

void MediaScheduledDatagramSenderNode::closeSender(
    ::media::ErrorInfo cause) noexcept
{
    if (m_session && !m_terminalFailure) {
        auto now = m_clock ? m_clock->now()
                           : ::media::Result<MediaRunningTime>::failure(cause);
        if (now) m_session->abort(std::move(cause), now.value());
    }
    m_session.reset();
    m_pendingBatch.reset();
    m_queuedWireBatches.clear();
    m_queuedWireDatagrams = 0;
    m_queuedWireBytes = 0;
}

::media::Status MediaScheduledDatagramSenderNode::stop(
    MediaGraphExecutionContext& context)
{
    m_stopSource.request_stop();
    m_wakeup.interrupt();
    std::optional<::media::ErrorInfo> closeFailure;
    if (m_pendingBatch && m_nextDatagram != 0 && !m_commitAttempted) {
        auto committed = commitAccumulatedSubmittedPrefix();
        if (!committed) closeFailure = committed.error();
    }
    if (m_session) {
        auto now = m_clock->now();
        if (!now) {
            if (!closeFailure) closeFailure = now.error();
        } else {
            auto closed = m_session->close(now.value());
            if (!closed && !closeFailure) closeFailure = closed.error();
        }
    }
    emitDiagnostics(closeFailure ? "failed" : "finished");
    m_session.reset();
    m_pendingBatch.reset();
    auto base = FFmpegNodeRuntime::stop(context);
    if (closeFailure) return ::media::Status::failure(*closeFailure);
    return base;
}

void MediaScheduledDatagramSenderNode::interrupt(
    MediaGraphExecutionContext&) noexcept
{
    m_stopSource.request_stop();
    m_wakeup.interrupt();
}

void MediaScheduledDatagramSenderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    interrupt(context);
    if (!m_terminalFailure) {
        auto cause = ::media::ErrorInfo::cancelled(
            "scheduled datagram sender was aborted");
        if (m_pendingBatch && m_nextDatagram != 0 && !m_commitAttempted) {
            auto committed = commitAccumulatedSubmittedPrefix();
            if (!committed) cause = committed.error();
        }
        m_terminalFailure = std::move(cause);
    }
    emitDiagnostics("aborted");
    closeSender(*m_terminalFailure);
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
