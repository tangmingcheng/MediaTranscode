#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
::media::Result<std::uint64_t> bytesForResidence(
    std::uint64_t bytesPerSecond, MediaRunningTime residence)
{
    return MediaCheckedArithmetic::bytesForResidence(
        bytesPerSecond, residence.nanoseconds(),
        "Datagram residence byte demand");
}

std::uint64_t ceilDivide(
    std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value / divisor + (value % divisor != 0 ? 1U : 0U);
}

::media::Result<std::uint64_t> smoothingPeakWireBytesPerSecond(
    const MediaWireTrafficEnvelope& wireTraffic,
    const MediaRealtimeDeploymentLatencyBudget& latency)
{
    auto serviceWindow = latency.targetResidence.checkedSubtract(
        latency.maximumReleaseJitter);
    if (!serviceWindow || serviceWindow.value().nanoseconds() <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram smoothing requires target residence greater than release jitter"));
    }
    auto burstRate = MediaCheckedArithmetic::ceilScale(
        wireTraffic.burstWireBytes,
        NanosecondsPerSecond,
        static_cast<std::uint64_t>(serviceWindow.value().nanoseconds()),
        "Datagram smoothing peak wire rate");
    if (!burstRate) {
        return burstRate;
    }
    return ::media::Result<std::uint64_t>::success(
        (std::max)(wireTraffic.peakWireBytesPerSecond,
                   burstRate.value()));
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
        ? MediaCheckedArithmetic::add(
              facts.service.burstWireBytes,
              peakServiceWithinDeadline.value(),
              "service burst within maximum residence")
        : peakServiceWithinDeadline;
    auto smoothingPeak = smoothingPeakWireBytesPerSecond(
        wireTraffic, facts.latency);
    if (sessionKey.empty() || remoteEndpoints.empty() ||
        wireTraffic.sustainedWireBytesPerSecond == 0 ||
        wireTraffic.peakWireBytesPerSecond <
            wireTraffic.sustainedWireBytesPerSecond ||
        wireTraffic.peakDatagramsPerSecond == 0 ||
        wireTraffic.burstWireBytes == 0 ||
        wireTraffic.burstDatagrams == 0 ||
        wireTraffic.maximumUdpPayloadBytes == 0 ||
        wireTraffic.maximumWireDatagramBytes == 0 ||
        wireTraffic.authority.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport template requires complete positive wire demand and endpoints"));
    }
    if (!smoothingPeak) {
        return Result::failure(smoothingPeak.error());
    }
    if (!burstServiceWithinDeadline) {
        return Result::failure(burstServiceWithinDeadline.error());
    }
    if (facts.service.sustainedWireBytesPerSecond <
        wireTraffic.sustainedWireBytesPerSecond) {
        std::ostringstream out;
        out << "Datagram sustained wire demand exceeds managed service: demand="
            << wireTraffic.sustainedWireBytesPerSecond
            << " service=" << facts.service.sustainedWireBytesPerSecond;
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(out.str()));
    }
    if (facts.service.peakWireBytesPerSecond < smoothingPeak.value()) {
        std::ostringstream out;
        out << "Datagram smoothing peak exceeds managed service: derived="
            << smoothingPeak.value()
            << " service=" << facts.service.peakWireBytesPerSecond
            << " wire_peak=" << wireTraffic.peakWireBytesPerSecond
            << " wire_burst=" << wireTraffic.burstWireBytes
            << " target_residence_ns="
            << facts.latency.targetResidence.nanoseconds()
            << " release_jitter_ns="
            << facts.latency.maximumReleaseJitter.nanoseconds();
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(out.str()));
    }
    if (burstServiceWithinDeadline.value() < wireTraffic.burstWireBytes) {
        std::ostringstream out;
        out << "Datagram wire burst exceeds service capacity within maximum residence: demand="
            << wireTraffic.burstWireBytes
            << " service_capacity=" << burstServiceWithinDeadline.value();
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(out.str()));
    }
    if (remoteEndpoints.size() > facts.localPorts.portCount) {
        std::ostringstream out;
        out << "Datagram endpoint count exceeds reserved local ports: endpoints="
            << remoteEndpoints.size()
            << " ports=" << facts.localPorts.portCount;
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(out.str()));
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
        auto smoothingPeak = smoothingPeakWireBytesPerSecond(
            m_encoding.wireTraffic, deployment.latency);
        if (!smoothingPeak) {
            return Result::failure(smoothingPeak.error());
        }
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
                resources.targetEffectiveSendBufferBytesPerEndpoint,
                resources.minimumEffectiveSendBufferBytesPerEndpoint,
                resources.maximumAdmittedEffectiveSendBufferBytesPerEndpoint});
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
                m_encoding.wireTraffic.sustainedWireBytesPerSecond,
                smoothingPeak.value(),
                m_encoding.wireTraffic.maximumWireDatagramBytes,
                m_encoding.wireTraffic.authority + "+" +
                    deployment.latency.authority + "+" +
                    deployment.latency.releaseJitterAuthority + "+" +
                    deployment.service.authority +
                    "+single-wire-datagram-transmit-quantum"},
            MediaDatagramBacklogPlan{
                resources.maximumBacklogDatagrams,
                resources.maximumBacklogBytes,
                deployment.latency.maximumResidence},
            MediaDatagramBatchPlan{
                resources.maximumBatchDatagrams,
                resources.maximumBatchBytes},
            MediaDatagramNetworkMemoryPlan{
                deployment.resources.maximumNetworkMemoryBytes,
                resources.admittedNetworkBytes,
                resources.admittedSocketBytes},
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
