#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRtpClockGroupBuffer::MediaRtpClockGroupBuffer(MediaRtpClockGroupSnapshot snapshot)
    : m_snapshot(std::move(snapshot))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    if (m_snapshot.state == MediaRtpClockGroupState::ReacquireRequired) {
        addFlags(MediaBufferFlag::Discontinuity);
    }
    setDiagnosticName("rtp.clock_group");
}

MediaBufferType MediaRtpClockGroupBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaRtpClockGroupSnapshot& MediaRtpClockGroupBuffer::snapshot() const noexcept
{
    return m_snapshot;
}

} // namespace media::ffmpeg::graph
