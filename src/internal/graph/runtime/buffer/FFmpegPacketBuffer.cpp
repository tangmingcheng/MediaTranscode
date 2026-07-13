#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"

#include <utility>
#include <limits>

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

std::optional<std::uint64_t> FFmpegPacketBuffer::payloadFootprintBytes() const noexcept
{
    if (!m_packet || m_packet->size < 0 ||
        (m_packet->size > 0 && !m_packet->data) ||
        m_packet->side_data_elems < 0 ||
        (m_packet->side_data_elems > 0 && !m_packet->side_data)) {
        return std::nullopt;
    }
    std::uint64_t bytes = static_cast<std::uint64_t>(m_packet->size);
    for (int index = 0; index < m_packet->side_data_elems; ++index) {
        if (m_packet->side_data[index].size > 0 &&
            !m_packet->side_data[index].data) {
            return std::nullopt;
        }
        const auto sideBytes = static_cast<std::uint64_t>(
            m_packet->side_data[index].size);
        if (sideBytes > std::numeric_limits<std::uint64_t>::max() - bytes) {
            return std::nullopt;
        }
        bytes += sideBytes;
    }
    return bytes == 0 ? std::nullopt
                      : std::optional<std::uint64_t>(bytes);
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
