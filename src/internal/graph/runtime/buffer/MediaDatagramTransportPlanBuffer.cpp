#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/diagnostics/MediaOutputPreparationObservation.h"

#include <limits>
#include <new>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramTransportPlanBuffer::MediaDatagramTransportPlanBuffer(
    MediaDatagramTransportPlan plan,
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence,
    std::vector<MediaDatagramRemoteEndpointFact> endpoints) noexcept
    : m_plan(std::move(plan)),
      m_globalSequence(std::move(globalSequence)),
      m_endpoints(std::move(endpoints))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::DatagramTransportPlan);
    setDiagnosticName("datagram_transport_plan");
}

::media::Result<MediaBufferRef> MediaDatagramTransportPlanBuffer::create(
    const MediaDatagramTransportPlanTemplate& planTemplate,
    std::uint64_t generation,
    std::shared_ptr<const MediaStorageReleaseNotification> releaseNotification)
{
    const auto& producer = planTemplate.encode().deployment.resources.mpegTsProducer;
    const auto& planned = planTemplate.encode().deployment.resources;
    MediaOutputPreparationObservation observation("transport_buffer", generation,
        planned.maximumNetworkMemoryBytes, planTemplate.remoteEndpoints().size());
    if (producer && !producer->muxQueueStorage) return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::notInitialized("Runtime TS transport requires final graph storage admission"));
    auto activated = [&] {
        MediaOutputPreparationObservation activationObservation("transport_activate", generation,
            planned.maximumNetworkMemoryBytes, planTemplate.remoteEndpoints().size());
        return planTemplate.activate(generation);
    }();
    if (!activated) {
        return ::media::Result<MediaBufferRef>::failure(activated.error());
    }
    const auto userspaceBytes = activated.value().shaping.networkMemory().reservedUserspaceBytes;
    if (userspaceBytes > (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
            "Datagram userspace storage budget is not addressable"));
    }
    auto storageBudget = MediaNetworkStorageBudget::create(
        static_cast<std::size_t>(userspaceBytes),
        MediaStandaloneStorage<MediaNetworkStorageDomain>{}, releaseNotification);
    if (!storageBudget) return ::media::Result<MediaBufferRef>::failure(storageBudget.error());
    auto storageResource = MediaNetworkMemoryResource::create(std::move(storageBudget).value());
    if (!storageResource) return ::media::Result<MediaBufferRef>::failure(storageResource.error());
    std::unordered_map<std::uint64_t, std::uint64_t> endpointWireHeaders;
    try {
        endpointWireHeaders.reserve(
            activated.value().shaping.endpoints().size());
        for (const auto& endpoint :
             activated.value().shaping.endpoints()) {
            if (endpoint.mtuEvidence.ipHeaderBytes >
                (std::numeric_limits<std::uint64_t>::max)() -
                    endpoint.mtuEvidence.transportHeaderBytes) {
                return ::media::Result<MediaBufferRef>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Datagram transport endpoint wire header overflows"));
            }
            endpointWireHeaders.emplace(
                endpoint.endpointId,
                endpoint.mtuEvidence.ipHeaderBytes +
                    endpoint.mtuEvidence.transportHeaderBytes);
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Datagram transport endpoint wire headers"));
    }
    auto sequence = MediaWireGlobalSequenceState::create(
        planTemplate.sessionKey(), planTemplate.serviceScopeId(), generation, 1,
        activated.value().shaping.backlog().maximumDatagrams,
        activated.value().shaping.backlog().maximumBytes,
        activated.value().shaping.backlog().commitBeforeNextReservation,
        std::move(storageResource).value(), std::move(endpointWireHeaders),
        activated.value().rtpStorageInstances, activated.value().commitStorage,
        producer ? producer->muxQueueStorage : std::nullopt,
        producer ? producer->seriesStorage : std::nullopt);
    if (!sequence) {
        return ::media::Result<MediaBufferRef>::failure(sequence.error());
    }
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints(
            planTemplate.remoteEndpoints());
        return ::media::Result<MediaBufferRef>::success(
            MediaBufferRef(new MediaDatagramTransportPlanBuffer(
                std::move(activated).value(), std::move(sequence).value(),
                std::move(endpoints))));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Datagram transport plan buffer"));
    }
}

::media::Result<std::uint64_t> MediaDatagramTransportPlanBuffer::endpointId(
    MediaDatagramProtocolEndpointRole role) const noexcept
{
    if (role == MediaDatagramProtocolEndpointRole::Unknown) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram transport buffer endpoint role must be explicit"));
    }
    for (const auto& endpoint : m_endpoints) {
        if (endpoint.role == role) {
            return ::media::Result<std::uint64_t>::success(
                endpoint.endpointId);
        }
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::notInitialized(
            "Datagram transport buffer endpoint role is absent"));
}

} // namespace media::ffmpeg::graph
