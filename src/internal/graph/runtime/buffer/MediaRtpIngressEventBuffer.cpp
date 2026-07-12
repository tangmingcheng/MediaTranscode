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

MediaRtpIngressEventBuffer::MediaRtpIngressEventBuffer(MediaRtpDiscontinuity discontinuity)
    : m_kind(MediaRtpIngressEventKind::Discontinuity)
    , m_discontinuity(discontinuity)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    addFlags(MediaBufferFlag::Discontinuity);
    setDiagnosticName("rtp.discontinuity");
}

MediaBufferType MediaRtpIngressEventBuffer::type() const noexcept { return MediaBufferType::Event; }
MediaRtpIngressEventKind MediaRtpIngressEventBuffer::kind() const noexcept { return m_kind; }
const std::optional<MediaRtcpClockEvidence>& MediaRtpIngressEventBuffer::clockEvidence() const noexcept { return m_clockEvidence; }
const std::optional<MediaRtpDiscontinuity>& MediaRtpIngressEventBuffer::discontinuity() const noexcept { return m_discontinuity; }

} // namespace media::ffmpeg::graph
