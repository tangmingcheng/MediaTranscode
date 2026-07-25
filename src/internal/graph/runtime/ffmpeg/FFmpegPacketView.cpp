#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"

namespace media::ffmpeg::graph {

AVPacket* FFmpegPacketView::writablePacket(const MediaBufferRef& buffer) noexcept
{
    if (auto* canonical = dynamic_cast<MediaCanonicalAccessUnitBuffer*>(buffer.get()))
        return writablePacket(canonical->media());
    if (auto* encoded = dynamic_cast<MediaEncodedAudioLineageBuffer*>(buffer.get()))
        return writablePacket(encoded->media());
    auto* packetBuffer = dynamic_cast<FFmpegPacketBuffer*>(buffer.get());
    return packetBuffer ? packetBuffer->packet() : nullptr;
}

const AVPacket* FFmpegPacketView::packet(const MediaBufferRef& buffer) noexcept
{
    if (const auto* canonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(buffer.get()))
        return packet(canonical->media());
    if (const auto* encoded = dynamic_cast<const MediaEncodedAudioLineageBuffer*>(buffer.get()))
        return packet(encoded->media());
    const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    return packetBuffer ? packetBuffer->packet() : nullptr;
}

std::shared_ptr<const MediaCanonicalLineage>
FFmpegPacketView::canonicalLineage(const MediaBufferRef& buffer) noexcept
{
    const auto* canonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(buffer.get());
    return canonical ? canonical->lineage() : nullptr;
}

bool FFmpegPacketView::isPacket(const MediaBufferRef& buffer) noexcept
{
    return packet(buffer) != nullptr;
}

} // namespace media::ffmpeg::graph
