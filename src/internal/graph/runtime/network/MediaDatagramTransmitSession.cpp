#include "internal/graph/runtime/network/MediaDatagramTransmitSession.h"

#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramTransmitSession::MediaDatagramTransmitSession(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation)
{
}

MediaDatagramTransmitSession::~MediaDatagramTransmitSession() noexcept
{
    close();
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
        bindings.size() != plan.endpoints().size()) {
        return ResultType::failure(::media::ErrorInfo::invalidArgument(
            "invalid explicit Datagram transmit execution plan"));
    }
    try {
        std::unordered_map<std::uint64_t, MediaUdpDatagramEndpoint> localById;
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
        std::vector<MediaDatagramTransmitPort*> evidencePorts;
        evidencePorts.reserve(plan.endpoints().size());
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
                execution.mode, plan.evidence()};
            auto opened = port.value()->open(request);
            if (!opened) return ResultType::failure(opened.error());
            if (opened.value().zeroCopyEnabled) {
                return ResultType::failure(::media::ErrorInfo::unsupported(
                    "Datagram MSG_ZEROCOPY is forbidden by the transmit plan"));
            }
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
            evidencePorts.push_back(portView);
        }
        auto collector = MediaDatagramTransmitEvidenceCollector::create(
            plan.generation(), plan.evidence(), std::move(evidencePorts));
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

::media::Result<MediaDatagramTransmitAttempt>
MediaDatagramTransmitSession::trySubmit(
    std::uint64_t endpointId,
    std::span<const MediaDatagramTransmitRequest> requests,
    MediaRunningTime submittedAt) noexcept
{
    using ResultType = ::media::Result<MediaDatagramTransmitAttempt>;
    if (m_terminalFailure) return ResultType::failure(*m_terminalFailure);
    const auto endpoint = m_endpoints.find(endpointId);
    if (m_closed || endpoint == m_endpoints.end() || requests.empty()) {
        auto error = ::media::ErrorInfo::invalidArgument(
            "invalid Datagram transmit submission");
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    const auto batchDeadline = requests.front().enqueueNotAfter;
    std::uint64_t batchWireBytes = 0;
    for (const auto& request : requests) {
        if (request.bytes.empty() ||
            request.bytes.size() > endpoint->second.maximumDatagramBytes ||
            request.enqueueNotAfter != batchDeadline ||
            request.enqueueNotAfter.nanoseconds() < 0 ||
            request.bytes.size() >
                (std::numeric_limits<std::uint64_t>::max)() -
                    endpoint->second.wireOverheadBytes) {
            auto error = ::media::ErrorInfo::invalidArgument(
                "Datagram transmit batch violates endpoint or deadline contract");
            terminate(error);
            return ResultType::failure(std::move(error));
        }
        const auto wireBytes = static_cast<std::uint64_t>(request.bytes.size()) +
                               endpoint->second.wireOverheadBytes;
        if (batchWireBytes > (std::numeric_limits<std::uint64_t>::max)() -
                                 wireBytes) {
            auto error = ::media::ErrorInfo::invalidArgument(
                "Datagram transmit batch wire accounting overflowed");
            terminate(error);
            return ResultType::failure(std::move(error));
        }
        batchWireBytes += wireBytes;
    }
    if (batchWireBytes > m_burstWireBytes) {
        auto error = ::media::ErrorInfo::invalidArgument(
            "Datagram transmit batch exceeds planner burst contract");
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    auto submitted = endpoint->second.port->trySubmit(requests);
    if (!submitted) {
        auto error = submitted.error();
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    if (submitted.value() == MediaDatagramTransmitAttempt::WouldBlock) {
        return submitted;
    }
    for (const auto& request : requests) {
        auto recorded = m_evidence->recordSubmitted(
            endpointId, request.evidenceId, submittedAt);
        if (!recorded) {
            auto error = recorded.error();
            terminate(error);
            return ResultType::failure(std::move(error));
        }
    }
    return submitted;
}

::media::Result<MediaDatagramWritableWaitResult>
MediaDatagramTransmitSession::waitWritable(
    std::uint64_t endpointId,
    MediaRunningTime maximumWait) noexcept
{
    using ResultType = ::media::Result<MediaDatagramWritableWaitResult>;
    if (m_terminalFailure) return ResultType::failure(*m_terminalFailure);
    const auto endpoint = m_endpoints.find(endpointId);
    if (m_closed || endpoint == m_endpoints.end() ||
        maximumWait.nanoseconds() < 0) {
        return ResultType::failure(::media::ErrorInfo::invalidArgument(
            "invalid Datagram writable wait"));
    }
    auto waited = endpoint->second.port->waitWritable(maximumWait);
    if (!waited) {
        auto error = waited.error();
        terminate(error);
        return ResultType::failure(std::move(error));
    }
    return waited;
}

::media::Status MediaDatagramTransmitSession::drainAvailableEvidence(
    MediaRunningTime now) noexcept
{
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    auto drained = m_evidence->drainAvailable(now);
    if (!drained) return terminate(drained.error());
    return drained;
}

::media::Status MediaDatagramTransmitSession::abort(
    ::media::ErrorInfo cause) noexcept
{
    if (cause.ok()) {
        cause = ::media::ErrorInfo::cancelled(
            "Datagram transmit session aborted");
    }
    return terminate(std::move(cause));
}

::media::Status MediaDatagramTransmitSession::close() noexcept
{
    if (m_closed) {
        return m_terminalFailure
            ? ::media::Status::failure(*m_terminalFailure)
            : ::media::Status::success();
    }
    m_closed = true;
    for (auto& [id, endpoint] : m_endpoints) {
        (void)id;
        auto closed = endpoint.port->close();
        if (!closed && !m_terminalFailure) m_terminalFailure = closed.error();
    }
    return m_terminalFailure
        ? ::media::Status::failure(*m_terminalFailure)
        : ::media::Status::success();
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

::media::Status MediaDatagramTransmitSession::terminate(
    ::media::ErrorInfo error) noexcept
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    close();
    return ::media::Status::failure(*m_terminalFailure);
}

} // namespace media::ffmpeg::graph
