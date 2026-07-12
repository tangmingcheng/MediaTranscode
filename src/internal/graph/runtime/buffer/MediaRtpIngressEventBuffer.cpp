#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"

namespace media::ffmpeg::graph {

MediaRtpIngressEventBuffer::MediaRtpIngressEventBuffer(MediaRtcpClockEvidence evidence)
    : m_kind(MediaRtpIngressEventKind::ClockEvidence)
    , m_clockEvidence(std::move(evidence))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("rtp.clock_evidence");
}

MediaRtpIngressEventBuffer::MediaRtpIngressEventBuffer(
    MediaRtpDiscontinuity discontinuity,
    std::uint64_t generation)
    : m_kind(MediaRtpIngressEventKind::Discontinuity)
    , m_discontinuity(discontinuity)
    , m_discontinuityGeneration(generation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    addFlags(MediaBufferFlag::Discontinuity);
    setDiagnosticName("rtp.discontinuity");
}

MediaRtpIngressEventBuffer::MediaRtpIngressEventBuffer(MediaRtpClockObservation observation)
    : m_kind(MediaRtpIngressEventKind::ClockObservation)
    , m_clockObservation(observation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("rtp.clock_observation");
}

MediaRtpIngressEventBuffer::MediaRtpIngressEventBuffer(MediaRtpClockInvalidation invalidation)
    : m_kind(MediaRtpIngressEventKind::ClockInvalidation)
    , m_clockInvalidation(invalidation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    addFlags(MediaBufferFlag::Discontinuity);
    setDiagnosticName("rtp.clock_invalidation");
}

MediaBufferType MediaRtpIngressEventBuffer::type() const noexcept { return MediaBufferType::Event; }
MediaRtpIngressEventKind MediaRtpIngressEventBuffer::kind() const noexcept { return m_kind; }
const std::optional<MediaRtcpClockEvidence>& MediaRtpIngressEventBuffer::clockEvidence() const noexcept { return m_clockEvidence; }
const std::optional<MediaRtpDiscontinuity>& MediaRtpIngressEventBuffer::discontinuity() const noexcept { return m_discontinuity; }
const std::optional<std::uint64_t>& MediaRtpIngressEventBuffer::discontinuityGeneration() const noexcept { return m_discontinuityGeneration; }
const std::optional<MediaRtpClockObservation>& MediaRtpIngressEventBuffer::clockObservation() const noexcept { return m_clockObservation; }
const std::optional<MediaRtpClockInvalidation>& MediaRtpIngressEventBuffer::clockInvalidation() const noexcept { return m_clockInvalidation; }

} // namespace media::ffmpeg::graph
