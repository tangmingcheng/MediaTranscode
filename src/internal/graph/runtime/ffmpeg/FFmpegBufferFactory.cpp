#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapInputFormatContext(::media::ffmpeg::InputFormatContextPtr context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapInputFormatContext failed: context is null"));
    }

    return ::media::Result<MediaBufferRef>::success(
        makeMediaBufferRef<FFmpegFormatContextBuffer>(std::move(context)));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapOutputFormatContext(::media::ffmpeg::OutputFormatContextPtr context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapOutputFormatContext failed: context is null"));
    }

    return ::media::Result<MediaBufferRef>::success(
        makeMediaBufferRef<FFmpegFormatContextBuffer>(std::move(context)));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::borrowFormatContext(AVFormatContext* context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("borrowFormatContext failed: context is null"));
    }

    return ::media::Result<MediaBufferRef>::success(
        makeMediaBufferRef<FFmpegFormatContextBuffer>(context));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapPacket(::media::ffmpeg::PacketPtr packet,
                                                                 MediaStreamKind streamKind)
{
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapPacket failed: packet is null"));
    }

    auto buffer = makeMediaBufferRef<FFmpegPacketBuffer>(std::move(packet));
    buffer->setStreamKind(streamKind);
    buffer->setPayloadKind(MediaPayloadKind::Packet);
    if (const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get())) {
        const AVPacket* avPacket = packetBuffer->packet();
        if (avPacket) {
            buffer->setTimestamps(avPacket->pts, avPacket->dts, avPacket->duration);
        }
    }

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::clonePacket(const AVPacket* packet,
                                                                  MediaStreamKind streamKind)
{
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("clonePacket failed: packet is null"));
    }

    auto cloned = ::media::ffmpeg::makePacket();
    if (!cloned) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("clonePacket failed: av_packet_alloc returned null"));
    }

    const int ret = av_packet_ref(cloned.get(), packet);
    if (ret < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(ret, "av_packet_ref"));
    }

    return wrapPacket(std::move(cloned), streamKind);
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapFrame(::media::ffmpeg::FramePtr frame,
                                                                MediaStreamKind streamKind)
{
    if (!frame) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapFrame failed: frame is null"));
    }

    const bool hardware = frame->hw_frames_ctx != nullptr;
    MediaBufferRef buffer;

    if (hardware) {
        MediaHardwareDescriptor hw;
        hw.frameKind = MediaHardwareFrameKind::Hardware;
        hw.zeroCopyPreferred = true;
        buffer = makeMediaBufferRef<HardwareFrameBuffer>(std::move(frame), std::move(hw));
    } else {
        buffer = makeMediaBufferRef<FFmpegFrameBuffer>(std::move(frame));
    }

    const auto* frameBuffer = dynamic_cast<const FFmpegFrameBuffer*>(buffer.get());
    const AVFrame* avFrame = frameBuffer ? frameBuffer->frame() : nullptr;
    if (avFrame) {
        buffer->setStreamKind(streamKind);
        buffer->setPayloadKind(MediaPayloadKind::Frame);
        buffer->setTimestamps(avFrame->pts, avFrame->pkt_dts, avFrame->duration);
        buffer->setFormatDescriptor(FFmpegDescriptorMapper::fromFrame(avFrame, streamKind));
    }

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::cloneFrame(const AVFrame* frame,
                                                                 MediaStreamKind streamKind)
{
    if (!frame) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("cloneFrame failed: frame is null"));
    }

    auto cloned = ::media::ffmpeg::makeFrame();
    if (!cloned) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("cloneFrame failed: av_frame_alloc returned null"));
    }

    const int ret = av_frame_ref(cloned.get(), frame);
    if (ret < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(ret, "av_frame_ref"));
    }

    return wrapFrame(std::move(cloned), streamKind);
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapHardwareFrame(::media::ffmpeg::FramePtr frame,
                                                                        MediaHardwareDescriptor hardware,
                                                                        MediaStreamKind streamKind)
{
    if (!frame) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapHardwareFrame failed: frame is null"));
    }

    auto buffer = makeMediaBufferRef<HardwareFrameBuffer>(std::move(frame), std::move(hardware));
    buffer->setStreamKind(streamKind);
    buffer->setPayloadKind(MediaPayloadKind::Frame);
    if (const auto* frameBuffer = dynamic_cast<const HardwareFrameBuffer*>(buffer.get())) {
        if (const AVFrame* avFrame = frameBuffer->frame()) {
            buffer->setTimestamps(avFrame->pts, avFrame->pkt_dts, avFrame->duration);
            buffer->setFormatDescriptor(FFmpegDescriptorMapper::fromFrame(avFrame, streamKind));
        }
    }

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

} // namespace media::ffmpeg::graph
