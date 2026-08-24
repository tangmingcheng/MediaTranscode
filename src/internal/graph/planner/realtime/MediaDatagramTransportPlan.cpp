#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"

#include "internal/graph/model/MediaNumericIpAddress.h"

#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramTransportPlanTemplate::MediaDatagramTransportPlanTemplate(
    MediaDatagramTransportPlanTemplateEncoding encoding) noexcept
    : m_encoding(std::move(encoding))
{
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaDatagramTransportPlanTemplate::create(
    std::string sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints)
{
    using Result = ::media::Result<MediaDatagramTransportPlanTemplate>;
    const auto& facts = deployment.encode();
    if (sessionKey.empty() || remoteEndpoints.empty() ||
        remoteEndpoints.size() > facts.localPorts.portCount) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport template requires a session and a reserved local port for every endpoint"));
    }
    try {
        std::unordered_set<std::uint64_t> endpointIds;
        endpointIds.reserve(remoteEndpoints.size());
        for (const auto& endpoint : remoteEndpoints) {
            if (endpoint.endpointId == 0 || endpoint.port == 0 ||
                endpoint.addressFamily != facts.localPorts.addressFamily ||
                !MediaNumericIpAddress::create(
                    endpoint.addressFamily, endpoint.numericAddress) ||
                !endpointIds.insert(endpoint.endpointId).second) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Datagram transport template requires unique numeric endpoints matching the reserved local address family"));
            }
        }
        return Result::success(MediaDatagramTransportPlanTemplate(
            MediaDatagramTransportPlanTemplateEncoding{
                std::move(sessionKey), facts, std::move(remoteEndpoints)}));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Datagram transport template"));
    }
}

::media::Result<MediaDatagramTransportPlan>
MediaDatagramTransportPlanTemplate::activate(std::uint64_t generation) const
{
    using Result = ::media::Result<MediaDatagramTransportPlan>;
    if (generation == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport activation generation must be positive"));
    }
    try {
        const auto& deployment = m_encoding.deployment;
        std::optional<MediaDatagramTransmitEvidencePlan> evidence;
        if (deployment.observation.evidencePolicy !=
            MediaRealtimeTransmitEvidencePolicy::Disabled) {
            evidence = MediaDatagramTransmitEvidencePlan{
                MediaDatagramTransmitEvidenceKind::TransmitTimestamp,
                deployment.observation.evidencePolicy ==
                        MediaRealtimeTransmitEvidencePolicy::Fail
                    ? MediaDatagramEvidenceCoverageGapPolicy::Fail
                    : MediaDatagramEvidenceCoverageGapPolicy::Report,
                deployment.observation.authority,
                1,
                deployment.observation.maximumRunDatagrams,
                deployment.observation.maximumCorrelationEntries,
                deployment.observation.maximumDrainResidence};
        }
        std::vector<MediaDatagramEndpointPlan> endpoints;
        std::vector<std::uint64_t> endpointCoverage;
        std::vector<MediaDatagramLocalEndpointPlan> localEndpoints;
        endpoints.reserve(m_encoding.remoteEndpoints.size());
        endpointCoverage.reserve(m_encoding.remoteEndpoints.size());
        localEndpoints.reserve(m_encoding.remoteEndpoints.size());
        for (std::size_t index = 0;
             index < m_encoding.remoteEndpoints.size(); ++index) {
            const auto& remote = m_encoding.remoteEndpoints[index];
            const auto localPort = static_cast<std::uint32_t>(
                deployment.localPorts.firstPort) +
                static_cast<std::uint32_t>(index);
            if (localPort > static_cast<std::uint32_t>(
                    (std::numeric_limits<std::uint16_t>::max)())) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Datagram local port allocation exceeds uint16 range"));
            }
            endpointCoverage.push_back(remote.endpointId);
            endpoints.push_back(MediaDatagramEndpointPlan{
                remote.endpointId,
                remote.addressFamily,
                remote.numericAddress,
                remote.port,
                deployment.mtu,
                deployment.mtu.senderMaximumPayloadBytes,
                deployment.resources.maximumEndpointPendingDatagrams,
                deployment.resources.maximumEndpointPendingBytes,
                deployment.resources.maximumResidence,
                deployment.resources.socketHardBoundBytes});
            localEndpoints.push_back(MediaDatagramLocalEndpointPlan{
                remote.endpointId,
                deployment.localPorts.addressFamily,
                deployment.localPorts.numericAddress,
                static_cast<std::uint16_t>(localPort)});
        }
        MediaDatagramShapingPlanEncoding shaping{
            m_encoding.sessionKey,
            generation,
            MediaDatagramServiceScopePlan{
                deployment.serviceScope.kind,
                deployment.serviceScope.scopeId,
                deployment.serviceScope.coverageAuthority,
                std::move(endpointCoverage)},
            std::move(endpoints),
            deployment.service,
            MediaDatagramBacklogPlan{
                deployment.resources.maximumBacklogDatagrams,
                deployment.resources.maximumBacklogBytes,
                deployment.resources.maximumResidence},
            MediaDatagramBatchPlan{
                deployment.resources.maximumBatchDatagrams,
                deployment.resources.maximumBatchBytes},
            MediaDatagramSubmitMode::NonBlockingAtomicEnqueue,
            MediaDatagramOrderingMode::CanonicalOrdered,
            MediaDatagramLimitFailureMode::Terminate,
            MediaDatagramLimitFailureMode::Terminate,
            MediaDatagramPersistentStateMode::PreserveScopeDebt,
            std::move(evidence)};
        auto decoded = MediaDatagramShapingPlan::decode(std::move(shaping));
        if (!decoded) return Result::failure(decoded.error());
        return Result::success(MediaDatagramTransportPlan{
            std::move(decoded).value(),
            std::move(localEndpoints),
            MediaDatagramTransportExecutionKind::UserspaceNonblocking,
            deployment.localPorts.authority});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "activated Datagram transport plan"));
    }
}

const std::string& MediaDatagramTransportPlanTemplate::sessionKey() const noexcept
{
    return m_encoding.sessionKey;
}

const std::string&
MediaDatagramTransportPlanTemplate::serviceScopeId() const noexcept
{
    return m_encoding.deployment.serviceScope.scopeId;
}

const std::vector<MediaDatagramRemoteEndpointFact>&
MediaDatagramTransportPlanTemplate::remoteEndpoints() const noexcept
{
    return m_encoding.remoteEndpoints;
}

} // namespace media::ffmpeg::graph
