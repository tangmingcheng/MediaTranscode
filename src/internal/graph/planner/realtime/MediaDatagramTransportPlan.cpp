#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"
#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"

#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
::media::Result<std::uint64_t> bytesForResidence(
    std::uint64_t bytesPerSecond, MediaRunningTime residence)
{
    return MediaRealtimePlanningArithmetic::bytesForResidence(
        bytesPerSecond, residence, "Datagram residence byte demand");
}

std::uint64_t ceilDivide(
    std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value / divisor + (value % divisor != 0 ? 1U : 0U);
}

} // namespace

MediaDatagramTransportPlanTemplate::MediaDatagramTransportPlanTemplate(
    MediaDatagramTransportPlanTemplateEncoding encoding) noexcept
    : m_encoding(std::move(encoding))
{
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaDatagramTransportPlanTemplate::create(
    std::string sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints,
    MediaWireTrafficEnvelope wireTraffic)
{
    using Result = ::media::Result<MediaDatagramTransportPlanTemplate>;
    const auto& facts = deployment.encode();
    auto peakServiceWithinDeadline = bytesForResidence(
        facts.service.peakWireBytesPerSecond,
        facts.latency.maximumResidence);
    auto burstServiceWithinDeadline = peakServiceWithinDeadline
        ? MediaRealtimePlanningArithmetic::add(
              facts.service.burstWireBytes,
              peakServiceWithinDeadline.value(),
              "service burst within maximum residence")
        : peakServiceWithinDeadline;
    if (sessionKey.empty() || remoteEndpoints.empty() ||
        wireTraffic.sustainedWireBytesPerSecond == 0 ||
        wireTraffic.peakWireBytesPerSecond <
            wireTraffic.sustainedWireBytesPerSecond ||
        wireTraffic.peakDatagramsPerSecond == 0 ||
        wireTraffic.burstWireBytes == 0 ||
        wireTraffic.burstDatagrams == 0 ||
        wireTraffic.maximumUdpPayloadBytes == 0 ||
        wireTraffic.maximumWireDatagramBytes == 0 ||
        wireTraffic.authority.empty() ||
        facts.service.sustainedWireBytesPerSecond <
            wireTraffic.sustainedWireBytesPerSecond ||
        facts.service.peakWireBytesPerSecond <
            wireTraffic.peakWireBytesPerSecond ||
        !burstServiceWithinDeadline ||
        burstServiceWithinDeadline.value() < wireTraffic.burstWireBytes ||
        remoteEndpoints.size() > facts.localPorts.portCount) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport template requires admitted wire demand, a session, and a reserved local port for every endpoint"));
    }
    try {
        std::unordered_set<std::uint64_t> endpointIds;
        std::unordered_set<std::uint8_t> endpointRoles;
        endpointIds.reserve(remoteEndpoints.size());
        endpointRoles.reserve(remoteEndpoints.size());
        for (const auto& endpoint : remoteEndpoints) {
            if (endpoint.endpointId == 0 || endpoint.port == 0 ||
                endpoint.role == MediaDatagramProtocolEndpointRole::Unknown ||
                endpoint.addressFamily != facts.localPorts.addressFamily ||
                !MediaNumericIpAddress::create(
                    endpoint.addressFamily, endpoint.numericAddress) ||
                !endpointIds.insert(endpoint.endpointId).second ||
                !endpointRoles.insert(
                    static_cast<std::uint8_t>(endpoint.role)).second) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Datagram transport template requires unique numeric endpoints matching the reserved local address family"));
            }
        }
        return Result::success(MediaDatagramTransportPlanTemplate(
            MediaDatagramTransportPlanTemplateEncoding{
                std::move(sessionKey), facts, std::move(remoteEndpoints),
                std::move(wireTraffic)}));
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
        const auto endpointCount = static_cast<std::uint64_t>(
            m_encoding.remoteEndpoints.size());
        auto resourceLedger =
            MediaRealtimeNetworkResourceLedgerPlanner::plan(
                deployment, m_encoding.wireTraffic, endpointCount);
        if (!resourceLedger) {
            return Result::failure(resourceLedger.error());
        }
        const auto& resources = resourceLedger.value();
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
                resources.maximumCorrelationEntries,
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
                MediaDatagramMtuEvidence{
                    deployment.mtu.authority,
                    deployment.mtu.maximumIpPacketBytes,
                    deployment.mtu.addressFamily == MediaIpAddressFamily::Ipv4
                        ? Ipv4HeaderBytes : Ipv6HeaderBytes,
                    UdpHeaderBytes,
                    deployment.mtu.senderMaximumPayloadBytes},
                m_encoding.wireTraffic.maximumUdpPayloadBytes,
                resources.maximumEndpointPendingDatagrams,
                resources.maximumEndpointPendingBytes,
                deployment.latency.maximumResidence,
                resources.socketHardBoundBytesPerEndpoint});
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
            MediaDatagramServiceCurvePlan{
                deployment.service.sustainedWireBytesPerSecond,
                deployment.service.peakWireBytesPerSecond,
                deployment.service.burstWireBytes,
                deployment.service.authority},
            MediaDatagramBacklogPlan{
                resources.maximumBacklogDatagrams,
                resources.maximumBacklogBytes,
                deployment.latency.maximumResidence},
            MediaDatagramBatchPlan{
                resources.maximumBatchDatagrams,
                resources.maximumBatchBytes},
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

::media::Result<std::uint64_t>
MediaDatagramTransportPlanTemplate::endpointId(
    MediaDatagramProtocolEndpointRole role) const noexcept
{
    if (role == MediaDatagramProtocolEndpointRole::Unknown) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram endpoint role must be explicit"));
    }
    for (const auto& endpoint : m_encoding.remoteEndpoints) {
        if (endpoint.role == role) {
            return ::media::Result<std::uint64_t>::success(
                endpoint.endpointId);
        }
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::notInitialized(
            "Datagram endpoint role is absent from the transport template"));
}

} // namespace media::ffmpeg::graph
