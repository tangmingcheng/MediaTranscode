#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"

#include "internal/graph/model/MediaNumericIpAddress.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

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
    const bool validScope =
        (scope.kind == MediaDatagramServiceScopeKind::ManagedEgress ||
         scope.kind == MediaDatagramServiceScopeKind::ProvisionedEgress) &&
        !scope.scopeId.empty() && !scope.coverageAuthority.empty();
    const bool validMtu = !mtu.authority.empty() &&
        mtu.maximumIpPacketBytes > mtu.ipHeaderBytes + mtu.transportHeaderBytes &&
        mtu.senderMaximumPayloadBytes > 0 &&
        mtu.senderMaximumPayloadBytes <=
            mtu.maximumIpPacketBytes - mtu.ipHeaderBytes - mtu.transportHeaderBytes;
    const bool validService = !service.authority.empty() &&
        service.sustainedWireBytesPerSecond > 0 &&
        service.peakWireBytesPerSecond >= service.sustainedWireBytesPerSecond &&
        service.burstWireBytes > 0;
    const bool validResources = !resources.authority.empty() &&
        resources.maximumBacklogDatagrams > 0 &&
        resources.maximumBacklogBytes > 0 && positive(resources.maximumResidence) &&
        resources.maximumBatchDatagrams > 0 && resources.maximumBatchBytes > 0 &&
        resources.maximumEndpointPendingDatagrams > 0 &&
        resources.maximumEndpointPendingBytes > 0 &&
        resources.socketHardBoundBytes >= resources.maximumEndpointPendingBytes &&
        resources.maximumBatchDatagrams <= resources.maximumBacklogDatagrams &&
        resources.maximumBatchBytes <= resources.maximumBacklogBytes;
    const auto localAddress = MediaNumericIpAddress::create(
        localPorts.addressFamily, localPorts.numericAddress);
    const bool validLocalPorts = localAddress && localPorts.firstPort > 0 &&
        localPorts.portCount > 0 && !localPorts.authority.empty() &&
        static_cast<std::uint32_t>(localPorts.firstPort) +
                static_cast<std::uint32_t>(localPorts.portCount) - 1U <=
            static_cast<std::uint32_t>(
                (std::numeric_limits<std::uint16_t>::max)());
    const bool validLatency = !latency.authority.empty() &&
        positive(latency.targetResidence) &&
        latency.maximumResidence >= latency.targetResidence &&
        latency.maximumResidence <= resources.maximumResidence;
    const bool validObservation = !observation.authority.empty() &&
        observation.maximumRunDatagrams > 0 &&
        observation.maximumCorrelationEntries > 0 &&
        observation.maximumCorrelationEntries >=
            resources.maximumBacklogDatagrams &&
        observation.maximumCorrelationEntries <= observation.maximumRunDatagrams &&
        positive(observation.maximumDrainResidence) &&
        observation.evidencePolicy !=
            MediaRealtimeTransmitEvidencePolicy::Unknown;
    if (!validScope || !validMtu || !validService || !validResources ||
        !validLocalPorts ||
        !validLatency || !validObservation) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime deployment envelope requires authoritative service scope, MTU, service curve, resource, latency, and observation facts"));
    }
    return ::media::Result<MediaRealtimeDeploymentEnvelope>::success(
        MediaRealtimeDeploymentEnvelope(std::move(encoding)));
}

} // namespace media::ffmpeg::graph
