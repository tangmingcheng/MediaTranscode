#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"

#include "internal/graph/model/MediaNumericIpAddress.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t MaximumUdpLength = 65'535;
constexpr std::uint64_t MaximumIpv4PacketBytes = 65'535;
constexpr std::uint64_t MaximumIpv6PacketBytesWithoutJumbogram =
    MaximumUdpLength + Ipv6HeaderBytes;

bool positive(MediaRunningTime value) noexcept
{
    return value > MediaRunningTime::fromNanoseconds(0);
}

} // namespace

MediaRealtimeDeploymentEnvelope::MediaRealtimeDeploymentEnvelope(
    MediaRealtimeDeploymentEnvelopeEncoding encoding) noexcept
    : m_encoding(std::move(encoding))
{
}

::media::Result<MediaRealtimeDeploymentEnvelope>
MediaRealtimeDeploymentEnvelope::decode(
    MediaRealtimeDeploymentEnvelopeEncoding encoding)
{
    const auto& scope = encoding.serviceScope;
    const auto& mtu = encoding.mtu;
    const auto& service = encoding.service;
    const auto& resources = encoding.resources;
    const auto& latency = encoding.latency;
    const auto& observation = encoding.observation;
    const auto& localPorts = encoding.localPorts;
    const auto ipHeaderBytes = mtu.addressFamily == MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes
        : mtu.addressFamily == MediaIpAddressFamily::Ipv6
            ? Ipv6HeaderBytes
            : 0;
    const bool validScope =
        (scope.kind == MediaDatagramServiceScopeKind::ManagedEgress ||
         scope.kind == MediaDatagramServiceScopeKind::ProvisionedEgress) &&
        !scope.scopeId.empty() && !scope.coverageAuthority.empty();
    const bool validMtu = !mtu.authority.empty() && ipHeaderBytes > 0 &&
        mtu.maximumIpPacketBytes <=
            (mtu.addressFamily == MediaIpAddressFamily::Ipv4
                 ? MaximumIpv4PacketBytes
                 : MaximumIpv6PacketBytesWithoutJumbogram) &&
        mtu.maximumIpPacketBytes > ipHeaderBytes + UdpHeaderBytes &&
        mtu.senderMaximumPayloadBytes > 0 &&
        mtu.senderMaximumPayloadBytes <= MaximumUdpLength - UdpHeaderBytes &&
        mtu.senderMaximumPayloadBytes <=
            mtu.maximumIpPacketBytes - ipHeaderBytes - UdpHeaderBytes;
    const bool validService = !service.authority.empty() &&
        service.sustainedWireBytesPerSecond > 0 &&
        service.peakWireBytesPerSecond >= service.sustainedWireBytesPerSecond &&
        service.burstWireBytes > 0;
    const bool validResources = !resources.authority.empty() &&
        resources.graphResourceScope !=
            MediaRealtimeGraphResourceBudgetScope::Unknown &&
        resources.maximumGraphPayloadAndReservedStorageBytes > 0 &&
        resources.maximumNetworkMemoryBytes > 0 &&
        resources.maximumSocketMemoryBytes > 0;
    const auto localAddress = MediaNumericIpAddress::create(
        localPorts.addressFamily, localPorts.numericAddress);
    const bool validLocalPorts = localAddress &&
        localPorts.addressFamily == mtu.addressFamily && localPorts.firstPort > 0 &&
        localPorts.portCount > 0 && !localPorts.authority.empty() &&
        static_cast<std::uint32_t>(localPorts.firstPort) +
                static_cast<std::uint32_t>(localPorts.portCount) - 1U <=
            static_cast<std::uint32_t>(
                (std::numeric_limits<std::uint16_t>::max)());
    const bool validLatency = !latency.authority.empty() &&
        positive(latency.targetResidence) &&
        latency.maximumResidence >= latency.targetResidence;
    const bool validObservation = !observation.authority.empty() &&
        observation.maximumRunDatagrams > 0 &&
        positive(observation.maximumDrainResidence) &&
        observation.maximumDrainResidence <= latency.maximumResidence &&
        observation.evidencePolicy !=
            MediaRealtimeTransmitEvidencePolicy::Unknown;
    const bool validReceiverTiming = !encoding.receiverTiming ||
        (!encoding.receiverTiming->authority.empty() &&
         positive(encoding.receiverTiming->transportDecodeLead));
    if (!validScope || !validMtu || !validService || !validResources ||
        !validLocalPorts ||
        !validLatency || !validObservation || !validReceiverTiming) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime deployment envelope requires authoritative service scope, MTU, service curve, resource, latency, and observation facts"));
    }
    return ::media::Result<MediaRealtimeDeploymentEnvelope>::success(
        MediaRealtimeDeploymentEnvelope(std::move(encoding)));
}

} // namespace media::ffmpeg::graph
