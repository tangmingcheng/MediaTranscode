#include "internal/graph/runtime/network/MediaDatagramTransmitSession.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>
#include <unordered_set>

namespace media::ffmpeg::graph {

MediaDatagramTransmitSession::MediaDatagramTransmitSession(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_ownerThread(std::this_thread::get_id())
{
}

MediaDatagramTransmitSession::~MediaDatagramTransmitSession() noexcept
{
    closePorts();
}

::media::Status MediaDatagramTransmitSession::validateActivation(
    const MediaDatagramShapingPlan& plan,
    const std::vector<MediaDatagramTransmitEndpointBinding>& bindings,
    const MediaDatagramTransmitExecutionPlan& execution)
{
    if ((execution.mode !=
             MediaDatagramTransmitExecutionMode::UserspaceNonblocking &&
         execution.mode !=
             MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) ||
        execution.authority.empty() ||
        bindings.size() != plan.endpoints().size() ||
        (execution.mode ==
             MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) !=
            execution.kernelSchedule.has_value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "invalid explicit Datagram transmit activation plan"));
    }
    try {
        std::unordered_set<std::uint64_t> ids;
        ids.reserve(bindings.size());
        for (const auto& binding : bindings) {
            if (binding.endpointId == 0 ||
                !ids.insert(binding.endpointId).second ||
                !plan.endpoint(binding.endpointId)) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Datagram transmit activation bindings do not match the shaping plan"));
            }
        }
        return ::media::Status::success();
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "Datagram transmit activation validation"));
    }
}

::media::Result<std::unique_ptr<MediaDatagramTransmitSession>>
MediaDatagramTransmitSession::create(
    const MediaDatagramShapingPlan& plan,
    std::vector<MediaDatagramTransmitEndpointBinding> bindings,
    MediaDatagramTransmitExecutionPlan execution,
    MediaDatagramTransmitPortFactory& portFactory)
{
    using ResultType =
        ::media::Result<std::unique_ptr<MediaDatagramTransmitSession>>;
    if ((execution.mode !=
             MediaDatagramTransmitExecutionMode::UserspaceNonblocking &&
         execution.mode !=
             MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) ||
        execution.authority.empty() ||
        bindings.size() != plan.endpoints().size() ||
        (execution.mode ==
             MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) !=
            execution.kernelSchedule.has_value() ||
        (execution.kernelSchedule &&
         (execution.kernelSchedule->authority.empty() ||
          execution.kernelSchedule->maximumCorrelationEntries <
              plan.backlog().maximumDatagrams ||
          execution.kernelSchedule->maximumRunDatagrams == 0 ||
          execution.kernelSchedule->maximumRunDatagrams >
              static_cast<std::uint64_t>(
                  (std::numeric_limits<std::uint32_t>::max)()) + 1ULL ||
          execution.kernelSchedule->maximumErrorQueueResidence <=
              MediaRunningTime::fromNanoseconds(0) ||
          execution.kernelSchedule->maximumErrorQueueResidence >
              plan.backlog().maximumResidence ||
          execution.kernelSchedule->maximumScheduleAheadNanoseconds == 0 ||
          execution.kernelSchedule->maximumScheduleAheadNanoseconds >
              static_cast<std::uint64_t>(
                  (std::numeric_limits<std::int64_t>::max)()) ||
          !MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(
               execution.kernelSchedule->maximumScheduleAheadNanoseconds))
               .checkedAdd(
                   execution.kernelSchedule->maximumErrorQueueResidence))) ||
        (plan.evidence() &&
         plan.evidence()->lastEvidenceId >
             (std::numeric_limits<std::uint32_t>::max)())) {
        return ResultType::failure(::media::ErrorInfo::invalidArgument(
            "invalid explicit Datagram transmit execution plan"));
    }
    try {
        std::unordered_map<std::uint64_t, MediaUdpDatagramEndpoint> localById;
        localById.reserve(bindings.size());
        for (auto& binding : bindings) {
            if (!localById.emplace(binding.endpointId,
                                   std::move(binding.localEndpoint)).second) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "duplicate Datagram transmit endpoint binding"));
            }
        }
        auto session = std::unique_ptr<MediaDatagramTransmitSession>(
            new MediaDatagramTransmitSession(
                plan.sessionKey(), plan.serviceScope().scopeId,
                plan.generation()));
        session->m_burstWireBytes = plan.serviceCurve().burstWireBytes;
        session->m_maximumBatchDatagrams = plan.batch().maximumDatagrams;
        session->m_maximumBatchBytes = plan.batch().maximumBytes;
        session->m_endpoints.reserve(plan.endpoints().size());
        std::vector<MediaDatagramTransmitEvidenceEndpoint> evidenceEndpoints;
        evidenceEndpoints.reserve(plan.endpoints().size());
        for (const auto& endpoint : plan.endpoints()) {
            const auto local = localById.find(endpoint.endpointId);
            if (local == localById.end()) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "missing Datagram transmit endpoint binding"));
            }
            auto port = portFactory.create();
            if (!port) return ResultType::failure(port.error());
            MediaDatagramTransmitPortOpenRequest request{
                plan.sessionKey(), plan.serviceScope().scopeId,
                plan.generation(), endpoint, local->second,
                execution.mode, plan.evidence(), execution.kernelSchedule};
            auto opened = port.value()->open(request);
            if (!opened) return ResultType::failure(opened.error());
            if (opened.value().zeroCopyEnabled) {
                return ResultType::failure(::media::ErrorInfo::unsupported(
                    "Datagram MSG_ZEROCOPY is forbidden by the transmit plan"));
            }
            if (opened.value().targetEffectiveSendBufferBytes !=
                    endpoint.targetEffectiveSendBufferBytes ||
                opened.value().effectiveSendBufferBytes <
                    endpoint.minimumEffectiveSendBufferBytes ||
                opened.value().effectiveSendBufferBytes >
                    endpoint.maximumAdmittedEffectiveSendBufferBytes ||
                opened.value().effectiveSendBufferBytes >
                    plan.networkMemory().maximumSocketBytes -
                        session->m_effectiveSocketBytes ||
                opened.value().effectiveSendBufferBytes >
                    plan.networkMemory().maximumTotalBytes -
                        plan.networkMemory().reservedUserspaceBytes -
                        session->m_effectiveSocketBytes) {
                return ResultType::failure(::media::ErrorInfo::unsupported(
                    "effective Datagram socket memory exceeds the activated network ledger"));
            }
            session->m_effectiveSocketBytes +=
                opened.value().effectiveSendBufferBytes;
            if (execution.mode ==
                    MediaDatagramTransmitExecutionMode::LinuxSocketTxTime &&
                !opened.value().kernelTransmitTimeAvailable) {
                return ResultType::failure(::media::ErrorInfo::unsupported(
                    "required Linux SO_TXTIME capability is unavailable"));
            }
            if (plan.evidence() &&
                plan.evidence()->coverageGapPolicy ==
                    MediaDatagramEvidenceCoverageGapPolicy::Fail &&
                opened.value().timestampAvailability !=
                    MediaDatagramTransmitTimestampAvailability::Available) {
                return ResultType::failure(::media::ErrorInfo::unsupported(
                    "required asynchronous transmit timestamp is unavailable"));
            }
            auto* portView = port.value().get();
            EndpointState state{endpoint.maximumDatagramBytes,
                                endpoint.mtuEvidence.ipHeaderBytes +
                                    endpoint.mtuEvidence.transportHeaderBytes,
                                std::move(port.value()), opened.value()};
            session->m_endpoints.emplace(endpoint.endpointId,
                                         std::move(state));
            evidenceEndpoints.push_back(MediaDatagramTransmitEvidenceEndpoint{
                endpoint.endpointId, portView, opened.value()});
        }
        auto collector = MediaDatagramTransmitEvidenceCollector::create(
            plan.generation(),
            plan.backlog().maximumDatagrams, plan.evidence(),
            execution.kernelSchedule, std::move(evidenceEndpoints));
        if (!collector) return ResultType::failure(collector.error());
        session->m_evidence =
            std::make_unique<MediaDatagramTransmitEvidenceCollector>(
                std::move(collector.value()));
        return ResultType::success(std::move(session));
    } catch (const std::bad_alloc&) {
        return ResultType::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramTransmitSession"));
    }
}

MediaDatagramTransmitSubmitResult MediaDatagramTransmitSession::trySubmitNew(
    std::uint64_t endpointId,
    std::span<const MediaDatagramTransmitJobEntry> entries,
    MediaRunningTime now) noexcept
{
    auto owner = validateOwnerThread();
    if (!owner) return terminateSubmit(mediaDatagramTransmitError(owner.error()));
    if (m_terminalSubmitFailure) {
        return MediaDatagramTransmitSubmitResult::failure(
            *m_terminalSubmitFailure);
    }
    if (m_terminalFailure) {
        return MediaDatagramTransmitSubmitResult::failure(
            mediaDatagramTransmitError(*m_terminalFailure));
    }
    auto clock = advanceClock(now);
    if (!clock) return terminateSubmit(mediaDatagramTransmitError(clock.error()));
    const auto endpoint = m_endpoints.find(endpointId);
    if (m_closed || m_pending || endpoint == m_endpoints.end() ||
        entries.empty() ||
        entries.size() > m_maximumBatchDatagrams) {
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::invalidArgument(
                "invalid new Datagram transmit job")));
    }
    const auto deadline = entries.front().enqueueNotAfter;
    std::uint64_t payloadBytes = 0;
    std::uint64_t wireBytes = 0;
    std::vector<std::uint64_t> evidenceIds;
    std::vector<std::optional<std::uint64_t>> launchTimes;
    try {
        evidenceIds.reserve(entries.size());
        launchTimes.reserve(entries.size());
        for (const auto& entry : entries) {
            if (entry.bytes.empty() ||
                entry.bytes.size() > endpoint->second.maximumDatagramBytes ||
                entry.enqueueNotAfter != deadline || now > deadline ||
                entry.enqueueNotAfter.nanoseconds() < 0 ||
                entry.bytes.size() >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        endpoint->second.wireOverheadBytes) {
                return terminateSubmit(mediaDatagramTransmitError(
                    ::media::ErrorInfo::invalidArgument(
                        "Datagram transmit job violates endpoint or deadline")));
            }
            const auto size = static_cast<std::uint64_t>(entry.bytes.size());
            if (payloadBytes >
                    (std::numeric_limits<std::uint64_t>::max)() - size ||
                wireBytes > (std::numeric_limits<std::uint64_t>::max)() -
                    size - endpoint->second.wireOverheadBytes) {
                return terminateSubmit(mediaDatagramTransmitError(
                    ::media::ErrorInfo::invalidArgument(
                        "Datagram transmit batch accounting overflowed")));
            }
            payloadBytes += size;
            wireBytes += size + endpoint->second.wireOverheadBytes;
            evidenceIds.push_back(entry.evidenceId);
            launchTimes.push_back(entry.kernelTransmitTimeNanoseconds);
        }
    } catch (const std::bad_alloc&) {
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::allocationFailed(
                "Datagram transmit pending job")));
    }
    if (payloadBytes > m_maximumBatchBytes || wireBytes > m_burstWireBytes) {
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::invalidArgument(
                "Datagram transmit job exceeds planner batch or burst bound")));
    }
    auto reservations = m_evidence->reserveBeforeSubmit(
        endpointId, evidenceIds, launchTimes, now);
    if (!reservations) {
        return terminateSubmit(mediaDatagramTransmitError(
            reservations.error()));
    }
    try {
        auto pending = std::make_unique<PendingJob>();
        pending->endpointId = endpointId;
        pending->entries.assign(entries.begin(), entries.end());
        pending->portRequests.reserve(entries.size());
        for (std::size_t index = 0; index < entries.size(); ++index) {
            pending->portRequests.push_back(MediaDatagramTransmitPortRequest{
                entries[index].bytes,
                reservations.value()[index].platformCorrelationId,
                entries[index].kernelTransmitTimeNanoseconds});
        }
        pending->reservations = std::move(reservations.value());
        pending->requiresWritableWait = false;
        m_pending = std::move(pending);
    } catch (const std::bad_alloc&) {
        m_evidence->cancelPrepared(reservations.value(), 0);
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::allocationFailed(
                "Datagram transmit pending job")));
    }
    return submitPending(now);
}

MediaDatagramTransmitSubmitResult MediaDatagramTransmitSession::retryPending(
    MediaRunningTime now) noexcept
{
    auto owner = validateOwnerThread();
    if (!owner) return terminateSubmit(mediaDatagramTransmitError(owner.error()));
    if (m_terminalSubmitFailure) {
        return MediaDatagramTransmitSubmitResult::failure(
            *m_terminalSubmitFailure);
    }
    if (m_terminalFailure) {
        return MediaDatagramTransmitSubmitResult::failure(
            mediaDatagramTransmitError(*m_terminalFailure));
    }
    auto clock = advanceClock(now);
    if (!clock) return terminateSubmit(mediaDatagramTransmitError(clock.error()));
    if (!m_pending || m_pending->requiresWritableWait) {
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::invalidArgument(
                "Datagram retry requires a successful writable wait")));
    }
    return submitPending(now);
}

MediaDatagramTransmitSubmitResult MediaDatagramTransmitSession::submitPending(
    MediaRunningTime now) noexcept
{
    const auto deadline = m_pending->entries.front().enqueueNotAfter;
    if (now > deadline) {
        m_evidence->cancelPrepared(m_pending->reservations, 0);
        m_pending.reset();
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::ioFailure(
                "Datagram submit exceeded its original deadline")));
    }
    auto evidence = m_evidence->drainAvailable(now);
    if (!evidence) {
        return terminateSubmit(mediaDatagramTransmitError(evidence.error()));
    }
    auto& endpoint = m_endpoints.at(m_pending->endpointId);
    auto submitted = endpoint.port->trySubmit(m_pending->portRequests);
    if (!submitted) {
        auto error = submitted.error();
        const auto count = m_pending->reservations.size();
        const bool validNoSubmit =
            error.kind == MediaDatagramTransmitFailureKind::TerminalNoSubmit &&
            error.submittedPrefixDatagrams == 0;
        const bool validPartial =
            error.kind ==
                MediaDatagramTransmitFailureKind::PartialSubmittedPrefix &&
            error.submittedPrefixDatagrams > 0 &&
            error.submittedPrefixDatagrams < count;
        const bool validAmbiguous =
            error.kind ==
                MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix &&
            error.submittedPrefixDatagrams < count;
        if (!validNoSubmit && !validPartial && !validAmbiguous) {
            error = mediaDatagramTransmitError(
                ::media::ErrorInfo::internalError(
                    "Datagram port returned invalid failure prefix metadata"),
                MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix, 0);
        }
        const auto prefix = error.submittedPrefixDatagrams;
        m_evidence->markSubmittedPrefix(m_pending->reservations, prefix);
        m_evidence->cancelPrepared(m_pending->reservations, prefix);
        m_pending.reset();
        return terminateSubmit(error);
    }
    if (submitted.value() == MediaDatagramTransmitAttempt::WouldBlock) {
        m_pending->requiresWritableWait = true;
        return submitted;
    }
    if (submitted.value() != MediaDatagramTransmitAttempt::Submitted) {
        return terminateSubmit(mediaDatagramTransmitError(
            ::media::ErrorInfo::internalError(
                "Datagram port returned an unknown submit result")));
    }
    m_evidence->markSubmittedPrefix(
        m_pending->reservations, m_pending->reservations.size());
    m_pending.reset();
    return submitted;
}

::media::Result<MediaDatagramWritableWaitResult>
MediaDatagramTransmitSession::waitWritable(
    std::uint64_t endpointId,
    MediaRunningTime now,
    MediaRunningTime maximumWait,
    std::stop_token stopToken) noexcept
{
    using ResultType = ::media::Result<MediaDatagramWritableWaitResult>;
    auto owner = validateOwnerThread();
    if (!owner) {
        terminate(owner.error());
        return ResultType::failure(owner.error());
    }
    if (m_terminalFailure) return ResultType::failure(*m_terminalFailure);
    auto clock = advanceClock(now);
    if (!clock) {
        terminate(clock.error());
        return ResultType::failure(clock.error());
    }
    const auto endpoint = m_endpoints.find(endpointId);
    if (m_closed || endpoint == m_endpoints.end() || !m_pending ||
        m_pending->endpointId != endpointId ||
        !m_pending->requiresWritableWait || maximumWait.nanoseconds() < 0) {
        auto error = ::media::ErrorInfo::invalidArgument(
            "invalid Datagram writable wait state");
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    auto remaining = m_pending->entries.front().enqueueNotAfter.checkedSubtract(now);
    if (!remaining || remaining.value().nanoseconds() < 0 ||
        maximumWait > remaining.value()) {
        auto error = ::media::ErrorInfo::invalidArgument(
            "Datagram writable wait exceeds original deadline");
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    auto waited = endpoint->second.port->waitWritable(maximumWait, stopToken);
    if (!waited) {
        terminate(waited.error());
        return ResultType::failure(waited.error());
    }
    if (waited.value() == MediaDatagramWritableWaitResult::Writable) {
        m_pending->requiresWritableWait = false;
    } else if (waited.value() != MediaDatagramWritableWaitResult::TimedOut &&
               waited.value() != MediaDatagramWritableWaitResult::Stopped) {
        auto error = ::media::ErrorInfo::internalError(
            "Datagram port returned an unknown writable wait result");
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    return waited;
}

::media::Status MediaDatagramTransmitSession::drainAvailableEvents(
    MediaRunningTime now) noexcept
{
    auto owner = validateOwnerThread();
    if (!owner) return terminate(owner.error());
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    auto clock = advanceClock(now);
    if (!clock) return terminate(clock.error());
    auto drained = m_evidence->drainAvailable(now);
    if (!drained) return terminate(drained.error());
    return drained;
}

::media::Status MediaDatagramTransmitSession::abort(
    ::media::ErrorInfo cause,
    MediaRunningTime now) noexcept
{
    auto owner = validateOwnerThread();
    if (!owner) return terminate(owner.error());
    auto clock = advanceClock(now);
    if (!clock) return terminate(clock.error());
    if (cause.ok()) {
        return terminate(::media::ErrorInfo::invalidArgument(
            "Datagram abort requires explicit worker-local causality"));
    }
    if (m_pending) {
        m_evidence->cancelPrepared(m_pending->reservations, 0);
        m_pending.reset();
    }
    return terminate(std::move(cause));
}

::media::Status MediaDatagramTransmitSession::close(
    MediaRunningTime now) noexcept
{
    auto owner = validateOwnerThread();
    if (!owner) return terminate(owner.error());
    if (m_closed) {
        return m_terminalFailure
            ? ::media::Status::failure(*m_terminalFailure)
            : ::media::Status::success();
    }
    auto clock = advanceClock(now);
    if (!clock && !m_terminalFailure) m_terminalFailure = clock.error();
    if (m_pending) {
        m_evidence->cancelPrepared(m_pending->reservations, 0);
        m_pending.reset();
    }
    auto settled = m_evidence->settleOnClose(now);
    if (!settled && !m_terminalFailure) m_terminalFailure = settled.error();
    closePorts();
    m_closed = true;
    return m_terminalFailure
        ? ::media::Status::failure(*m_terminalFailure)
        : ::media::Status::success();
}

::media::Status MediaDatagramTransmitSession::advanceClock(
    MediaRunningTime now) noexcept
{
    if (now.nanoseconds() < 0 || (m_lastNow && now < *m_lastNow)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transmit clock must be nonnegative and monotonic"));
    }
    m_lastNow = now;
    return ::media::Status::success();
}

::media::Status MediaDatagramTransmitSession::validateOwnerThread() noexcept
{
    if (std::this_thread::get_id() != m_ownerThread) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transmit session is a non-migrating single-owner object"));
    }
    return ::media::Status::success();
}

::media::Status MediaDatagramTransmitSession::terminate(
    ::media::ErrorInfo error) noexcept
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    if (m_pending) {
        m_evidence->cancelPrepared(m_pending->reservations, 0);
        m_pending.reset();
    }
    return ::media::Status::failure(*m_terminalFailure);
}

MediaDatagramTransmitSubmitResult MediaDatagramTransmitSession::terminateSubmit(
    MediaDatagramTransmitError error) noexcept
{
    if (!m_terminalSubmitFailure) m_terminalSubmitFailure = error;
    if (!m_terminalFailure) m_terminalFailure = error.cause;
    if (m_pending) {
        m_evidence->cancelPrepared(m_pending->reservations, 0);
        m_pending.reset();
    }
    return MediaDatagramTransmitSubmitResult::failure(
        *m_terminalSubmitFailure);
}

void MediaDatagramTransmitSession::closePorts() noexcept
{
    if (m_portsClosed) return;
    m_portsClosed = true;
    for (auto& [endpointId, endpoint] : m_endpoints) {
        auto closed = endpoint.port->close();
        if (!closed && !m_terminalFailure) m_terminalFailure = closed.error();
        (void)endpointId;
    }
}

const MediaDatagramTransmitEvidenceTelemetry&
MediaDatagramTransmitSession::evidenceTelemetry() const noexcept
{
    return m_evidence->telemetry();
}

const MediaDatagramTransmitPortCapabilities*
MediaDatagramTransmitSession::capabilities(std::uint64_t endpointId) const noexcept
{
    const auto endpoint = m_endpoints.find(endpointId);
    return endpoint == m_endpoints.end() ? nullptr : &endpoint->second.capabilities;
}

} // namespace media::ffmpeg::graph
