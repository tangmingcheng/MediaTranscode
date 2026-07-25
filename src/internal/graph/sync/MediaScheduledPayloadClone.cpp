#include "internal/graph/sync/MediaScheduledPayloadClone.h"

#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaScheduledPayloadClone::clonePacket(
    const MediaBufferRef& source)
{
    const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(source.get());
    if (!packetBuffer || !packetBuffer->packet()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled repeat requires an FFmpeg packet payload"));
    }
    ::media::ffmpeg::PacketPtr packet(av_packet_clone(packetBuffer->packet()));
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::internalError("Failed to clone scheduled packet"));
    }
    auto clone = makeMediaBufferRef<FFmpegPacketBuffer>(
        std::move(packet), packetBuffer->sourceTiming());
    clone->setStreamKind(source->streamKind());
    clone->setPayloadKind(source->payloadKind());
    clone->setFormatDescriptor(source->formatDescriptor());
    clone->setTimeDescriptor(source->timeDescriptor());
    clone->setHardwareDescriptor(source->hardwareDescriptor());
    clone->setTimestamps(source->pts(), source->dts(), source->duration());
    clone->setFlags(source->flags());
    clone->setDiagnosticName(source->diagnosticName());
    return ::media::Result<MediaBufferRef>::success(std::move(clone));
}

} // namespace media::ffmpeg::graph
