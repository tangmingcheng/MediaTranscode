#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <cstring>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> FFmpegBufferFactory::makeEof(MediaStreamKind streamKind)
{
    auto buffer = makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Eof);
    buffer->setStreamKind(streamKind);
    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::makeFlush(MediaStreamKind streamKind)
{
    auto buffer = makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Flush);
    buffer->setStreamKind(streamKind);
    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapInputFormatContext(::media::ffmpeg::InputFormatContextPtr context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapInputFormatContext failed: context is null"));
    }

    auto created = FFmpegFormatContextBuffer::createInput(std::move(context));
    if (!created) {
        return ::media::Result<MediaBufferRef>::failure(created.error());
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(std::move(created).value()));
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

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapCodecContext(::media::ffmpeg::CodecContextPtr context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapCodecContext failed: context is null"));
    }

    return ::media::Result<MediaBufferRef>::success(
        makeMediaBufferRef<FFmpegCodecContextBuffer>(std::move(context)));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::borrowCodecContext(AVCodecContext* context)
{
    if (!context) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("borrowCodecContext failed: context is null"));
    }

    return ::media::Result<MediaBufferRef>::success(
        makeMediaBufferRef<FFmpegCodecContextBuffer>(context));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::cloneCodecParameters(const AVStream* stream)
{
    if (!stream) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("cloneCodecParameters failed: stream is null"));
    }

    if (!stream->codecpar) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("cloneCodecParameters failed: stream codec parameters are null"));
    }

    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("cloneCodecParameters failed: avcodec_parameters_alloc returned null"));
    }

    const int copyRet = avcodec_parameters_copy(parameters.get(), stream->codecpar);
    if (copyRet < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(copyRet, "avcodec_parameters_copy"));
    }

    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(std::move(parameters));
    MediaFormatDescriptor descriptor = FFmpegDescriptorMapper::fromStream(stream);
    buffer->setStreamKind(descriptor.streamKind);
    buffer->setPayloadKind(MediaPayloadKind::CodecParameters);
    buffer->setFormatDescriptor(descriptor);

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = descriptor.time.timeBase;
    timeDescriptor.frameRate = descriptor.time.frameRate;
    timeDescriptor.startTime = descriptor.time.startTime;
    timeDescriptor.duration = descriptor.time.duration;
    buffer->setTimeDescriptor(timeDescriptor);

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::cloneCodecParameters(const FFmpegInputStreamSnapshot& stream)
{
    auto parameters = stream.cloneCodecParameters();
    if (!parameters) return ::media::Result<MediaBufferRef>::failure(parameters.error());
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(std::move(parameters).value());
    buffer->setStreamKind(stream.streamKind);
    buffer->setPayloadKind(MediaPayloadKind::CodecParameters);
    buffer->setFormatDescriptor(stream.format);
    buffer->setTimeDescriptor(stream.time);
    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegBufferFactory::wrapPacket(::media::ffmpeg::PacketPtr packet,
                                                                 MediaStreamKind streamKind,
                                                                 std::optional<MediaPacketSourceTiming> sourceTiming)
{
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("wrapPacket failed: packet is null"));
    }

    auto buffer = makeMediaBufferRef<FFmpegPacketBuffer>(std::move(packet), std::move(sourceTiming));
    buffer->setStreamKind(streamKind);
    buffer->setPayloadKind(MediaPayloadKind::Packet);
    if (const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get())) {
        if (const AVPacket* avPacket = packetBuffer->packet()) {
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

    if (packet->size > 0) {
        const int payloadRet = av_new_packet(cloned.get(), packet->size);
        if (payloadRet < 0) {
            return ::media::Result<MediaBufferRef>::failure(
                FFmpegGraphError::fromCode(payloadRet, "av_new_packet"));
        }
        if (packet->data) {
            std::memcpy(cloned->data, packet->data, static_cast<std::size_t>(packet->size));
        }
    }

    const int ret = av_packet_copy_props(cloned.get(), packet);
    if (ret < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(ret, "av_packet_copy_props"));
    }

    return wrapPacket(std::move(cloned), streamKind, std::nullopt);
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
