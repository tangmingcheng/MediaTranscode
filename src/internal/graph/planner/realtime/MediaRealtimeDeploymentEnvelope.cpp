#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"

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
    const bool validLatency = !latency.authority.empty() &&
        positive(latency.targetResidence) &&
        latency.maximumResidence >= latency.targetResidence &&
        latency.maximumResidence <= resources.maximumResidence;
    const bool validObservation = !observation.authority.empty() &&
        observation.maximumRunDatagrams > 0 &&
        observation.maximumCorrelationEntries > 0 &&
        observation.maximumCorrelationEntries <= observation.maximumRunDatagrams &&
        positive(observation.maximumDrainResidence);
    if (!validScope || !validMtu || !validService || !validResources ||
        !validLatency || !validObservation) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime deployment envelope requires authoritative service scope, MTU, service curve, resource, latency, and observation facts"));
    }
    return ::media::Result<MediaRealtimeDeploymentEnvelope>::success(
        MediaRealtimeDeploymentEnvelope(std::move(encoding)));
}

} // namespace media::ffmpeg::graph
