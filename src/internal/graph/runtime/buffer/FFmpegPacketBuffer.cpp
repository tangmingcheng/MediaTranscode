#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegPacketBuffer::FFmpegPacketBuffer(
    ::media::ffmpeg::PacketPtr packet,
    std::optional<MediaPacketSourceTiming> sourceTiming)
    : m_packet(std::move(packet))
    , m_sourceTiming(std::move(sourceTiming))
{
    setPayloadKind(MediaPayloadKind::Packet);
    if (m_packet && (m_packet->flags & AV_PKT_FLAG_KEY)) {
        addFlags(MediaBufferFlag::KeyFrame);
    }
}

const std::optional<MediaPacketSourceTiming>& FFmpegPacketBuffer::sourceTiming() const noexcept
{
    return m_sourceTiming;
}

MediaBufferType FFmpegPacketBuffer::type() const noexcept
{
    return MediaBufferType::Packet;
}

AVPacket* FFmpegPacketBuffer::packet() noexcept
{
    return m_packet.get();
}

const AVPacket* FFmpegPacketBuffer::packet() const noexcept
{
    return m_packet.get();
}

::media::ffmpeg::PacketPtr FFmpegPacketBuffer::takePacket() noexcept
{
    return std::move(m_packet);
}

} // namespace media::ffmpeg::graph
