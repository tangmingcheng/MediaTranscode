#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <chrono>
#include <limits>
#include <new>
#include <sstream>
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
    if (context.inputChannels(nodeId()).size() != 2 ||
        !context.outputChannels(nodeId()).empty() || !plan || !batch ||
        plan->binding().streamKind != MediaStreamKind::Metadata ||
        plan->binding().payloadKind != MediaPayloadKind::DatagramTransportPlan ||
        batch->binding().streamKind != MediaStreamKind::Metadata ||
        batch->binding().payloadKind !=
            MediaPayloadKind::ScheduledWireDatagramBatch) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires exact transport plan and scheduled wire inputs"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::start(
    MediaGraphExecutionContext& context)
{
    m_session.reset();
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
    m_burstWireBytes = 0;
    m_maximumBatchDatagrams = 0;
    m_maximumBatchBytes = 0;
    m_state = SubmitState::WaitReservation;
    m_nextDatagram = 0;
    m_groupBegin = 0;
    m_groupCount = 0;
    m_groupEndpointId = 0;
    m_groupNotBefore = MediaRunningTime::fromNanoseconds(0);
    m_groupDeadline = MediaRunningTime::fromNanoseconds(0);
    m_groupServiceDuration = MediaRunningTime::fromNanoseconds(0);
    m_nextPhysicalSubmitNotBefore.reset();
    m_lastSubmittedAt.reset();
    m_terminalFailure.reset();
    m_wakeup.reset();
    m_stopSource = std::stop_source{};
    m_batches = 0;
    m_datagrams = 0;
    m_bytes = 0;
    m_wouldBlockEvents = 0;
    m_writableWaits = 0;
    m_physicalSpacingDeferrals = 0;
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
    if (!activation || plan.shaping.sessionKey() != m_plannedSession.value() ||
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
    if (m_maximumBatchDatagrams > m_submitEntries.max_size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender batch capacity is not representable"));
    }
    try {
        m_submitEntries.reserve(
            static_cast<std::size_t>(m_maximumBatchDatagrams));
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

::media::Status MediaScheduledDatagramSenderNode::beginSubmitGroup()
{
    if (!m_pendingBatch || !m_generation || !m_session ||
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
    m_groupNotBefore = first.enqueueNotBefore();
    m_groupDeadline = first.enqueueNotAfter();
    m_groupServiceDuration = first.wireServiceDuration();
    if (m_nextPhysicalSubmitNotBefore &&
        *m_nextPhysicalSubmitNotBefore > m_groupNotBefore) {
        m_groupNotBefore = *m_nextPhysicalSubmitNotBefore;
        ++m_physicalSpacingDeferrals;
    }
    if (m_groupNotBefore > m_groupDeadline) {
        ++m_deadlineMisses;
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "scheduled datagram physical service spacing exceeds its original deadline"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::preflightBatchTelemetry(
    const MediaScheduledWireDatagramBatchBuffer& batch) const
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

void MediaScheduledDatagramSenderNode::recordSubmittedPrefix(
    std::size_t count,
    MediaRunningTime submittedAt) noexcept
{
    std::int64_t serviceNanoseconds = 0;
    for (std::size_t offset = 0; offset < count; ++offset) {
        serviceNanoseconds +=
            m_pendingBatch->m_datagrams[m_groupBegin + offset]
                .wireServiceDuration().nanoseconds();
    }
    m_nextPhysicalSubmitNotBefore = MediaRunningTime::fromNanoseconds(
        submittedAt.nanoseconds() + serviceNanoseconds);
    m_lastSubmittedAt = submittedAt;
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
    MediaRunningTime submittedAt)
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
        recordSubmittedPrefix(
            static_cast<std::size_t>(error.submittedPrefixDatagrams),
            submittedAt);
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
            auto begun = beginSubmitGroup();
            if (!begun) return failTerminal(begun.error());
            auto waited = waitUntil(m_groupNotBefore);
            if (!waited) return failTerminal(waited.error());
            m_state = SubmitState::TrySubmit;
        }

        if (m_state == SubmitState::WaitWritableWithinOriginalDeadline) {
            auto now = m_clock->now();
            if (!now) return failTerminal(now.error());
            if (now.value() >= m_groupDeadline) {
                ++m_deadlineMisses;
                return failTerminal(::media::ErrorInfo::ioFailure(
                    "scheduled datagram remained blocked through its original deadline"));
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
            auto nextPhysical = now.value().checkedAdd(m_groupServiceDuration);
            if (!nextPhysical) return failTerminal(nextPhysical.error());
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
                        datagram.enqueueNotAfter(), std::nullopt});
                }
                submitted = m_session->trySubmitNew(
                    m_groupEndpointId, m_submitEntries, now.value());
            }
            if (!submitted) return failSubmit(submitted.error(), now.value());
            if (submitted.value() == MediaDatagramTransmitAttempt::WouldBlock) {
                ++m_wouldBlockEvents;
                m_state = SubmitState::WaitWritableWithinOriginalDeadline;
                continue;
            }
            if (submitted.value() != MediaDatagramTransmitAttempt::Submitted) {
                return failTerminal(::media::ErrorInfo::internalError(
                    "scheduled datagram transport returned an unknown submit outcome"));
            }
            recordSubmittedPrefix(m_groupCount, now.value());
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
                   << " physical_spacing_deferrals="
                   << m_physicalSpacingDeferrals
                   << " deadline_misses=" << m_deadlineMisses
                   << " pressure_failures=" << m_pressureFailures
                   << " partial_submitted_failures="
                   << m_partialSubmittedFailures
                   << " ambiguous_submitted_failures="
                   << m_ambiguousSubmittedFailures
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

    auto batchInput = tryPopInputOptional(context, "batch");
    if (!batchInput) return failTerminal(batchInput.error());
    if (!batchInput.value()) {
        const auto* channel = context.findInputChannel(nodeId(), "batch");
        return channel && channel->closed() ? processFinished()
                                            : processWaiting();
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            batchInput.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Abort) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "scheduled datagram sender received abort"));
        }
        return control->controlKind() == MediaControlBufferKind::Eof
            ? processFinished() : processProgress();
    }
    auto batch = std::dynamic_pointer_cast<MediaScheduledWireDatagramBatchBuffer>(
        *batchInput.value());
    if (!batch || batch->sessionKey() != m_plannedSession.value() ||
        batch->serviceScopeId() != m_serviceScopeId || !m_generation ||
        batch->generation() != *m_generation || batch->m_datagrams.empty()) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender received a batch outside the active service scope"));
    }
    auto telemetry = preflightBatchTelemetry(*batch);
    if (!telemetry) return failTerminal(telemetry.error());
    m_pendingBatch = std::move(batch);
    m_nextDatagram = 0;
    m_lastSubmittedAt.reset();
    m_commitAttempted = false;
    m_state = SubmitState::WaitReservation;
    return progressPendingBatch();
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
