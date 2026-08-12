#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketPayloadFootprint.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegPacketBuffer::FFmpegPacketBuffer(
    ::media::ffmpeg::PacketPtr packet,
    std::optional<MediaPacketSourceTiming> sourceTiming,
    std::optional<MediaDemuxPacketProvenance> provenance)
    : m_packet(std::move(packet))
    , m_sourceTiming(std::move(sourceTiming))
    , m_demuxProvenance(std::move(provenance))
{
    setPayloadKind(MediaPayloadKind::Packet);
    if (m_packet && (m_packet->flags & AV_PKT_FLAG_KEY)) {
        addFlags(MediaBufferFlag::KeyFrame);
    }
}

const std::optional<MediaDemuxPacketProvenance>&
FFmpegPacketBuffer::demuxProvenance() const noexcept
{
    return m_demuxProvenance;
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
    return m_packet
        ? ffmpegPacketPayloadFootprintBytes(*m_packet)
        : std::nullopt;
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
