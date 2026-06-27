#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

AVPacket* FFmpegPacketView::writablePacket(const MediaBufferRef& buffer) noexcept
{
    auto* packetBuffer = dynamic_cast<FFmpegPacketBuffer*>(buffer.get());
    return packetBuffer ? packetBuffer->packet() : nullptr;
}

const AVPacket* FFmpegPacketView::packet(const MediaBufferRef& buffer) noexcept
{
    const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    return packetBuffer ? packetBuffer->packet() : nullptr;
}

bool FFmpegPacketView::isPacket(const MediaBufferRef& buffer) noexcept
{
    return packet(buffer) != nullptr;
}

} // namespace media::ffmpeg::graph
