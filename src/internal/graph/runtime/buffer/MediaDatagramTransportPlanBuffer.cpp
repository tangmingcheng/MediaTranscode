#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"

#include <new>
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
    std::uint64_t generation)
{
    auto activated = planTemplate.activate(generation);
    if (!activated) {
        return ::media::Result<MediaBufferRef>::failure(activated.error());
    }
    auto sequence = MediaWireGlobalSequenceState::create(
        planTemplate.sessionKey(), planTemplate.serviceScopeId(), generation, 1,
        activated.value().shaping.backlog().maximumDatagrams);
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
