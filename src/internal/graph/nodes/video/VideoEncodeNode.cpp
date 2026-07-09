#include "internal/graph/nodes/video/VideoEncodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

bool plannedHardwareEncoder(const MediaNodeOptions* options)
{
    return optionValue(options, "encoder.pipeline.frame_kind") == "hardware";
}

std::string pixelFormatName(int format)
{
    const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? std::string(name) : std::string("unknown");
}

std::string codecName(const AVCodecContext* context)
{
    if (!context) {
        return "unknown";
    }
    if (context->codec && context->codec->name) {
        return context->codec->name;
    }
    const char* name = avcodec_get_name(context->codec_id);
    return name ? std::string(name) : std::string("unknown");
}

void encodeLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_encode.") + message);
}

::media::Status validateFrameAgainstPlan(const MediaNodeOptions* options,
                                         const AVCodecContext* encoderContext,
                                         const AVFrame* frame)
{
    if (!encoderContext || !frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode requires valid encoder context and frame"));
    }

    if (!plannedHardwareEncoder(options)) {
        return ::media::Status::success();
    }

    if (!encoderContext->hw_frames_ctx) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode planner requires hardware encoder but encoder hw_frames_ctx is not set"));
    }

    if (!frame->hw_frames_ctx) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode planner requires hardware encoder but input frame is not hardware-backed"));
    }

    if (encoderContext->pix_fmt != AV_PIX_FMT_NONE && frame->format != encoderContext->pix_fmt) {
        std::ostringstream out;
        out << "VideoEncodeNode planner requires frame format " << pixelFormatName(encoderContext->pix_fmt)
            << " but input frame format is " << pixelFormatName(frame->format);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    return ::media::Status::success();
}

} // namespace

VideoEncodeNode::VideoEncodeNode(MediaNodeId nodeId)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "VideoEncodeNode")
{
}

MediaNodeKind VideoEncodeNode::staticKind() noexcept
{
    return MediaNodeKind::VideoEncode;
}

::media::Status VideoEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        const AVCodecContext* encoder = codecContext();
        encodeLog(MediaGraphDiagnosticLevel::State,
                  std::string("bind_encoder codec=") + codecName(encoder) +
                      " pix_fmt=" + pixelFormatName(encoder ? encoder->pix_fmt : AV_PIX_FMT_NONE) +
                      " frame_kind=" + optionValue(nodeOptions(context), "encoder.pipeline.frame_kind", "software") +
                      " hwaccel=" + optionValue(nodeOptions(context), "encoder.pipeline.hwaccel", "none") +
                      " hw_device_ctx=" + (encoder && encoder->hw_device_ctx ? "set" : "none") +
                      " hw_frames_ctx=" + (encoder && encoder->hw_frames_ctx ? "set" : "none"));
        return emitEncoderConfig(context, buffer);
    }

    if (!hasCodecContext()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoEncodeNode requires codec context before frames"));
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const int sendRet = avcodec_send_frame(codecContext(), nullptr);
        if (sendRet < 0 && sendRet != AVERROR_EOF) {
            return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video flush)");
        }
        auto drainStatus = receivePackets(context);
        if (!drainStatus) {
            return drainStatus;
        }
        return emitOutput(context, "packet", buffer);
    }

    AVFrame* frame = FFmpegFrameView::writableFrame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoEncodeNode expected frame buffer"));
    }

    auto validateStatus = validateFrameAgainstPlan(nodeOptions(context), codecContext(), frame);
    if (!validateStatus) {
        return validateStatus;
    }

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_encode.frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "frame seq=" << decision.sequence
            << " codec=" << codecName(codecContext())
            << " encoder_fmt=" << pixelFormatName(codecContext()->pix_fmt)
            << " frame_fmt=" << pixelFormatName(frame->format)
            << " frame_hw=" << (frame->hw_frames_ctx ? "set" : "none")
            << " encoder_hw_frames=" << (codecContext()->hw_frames_ctx ? "set" : "none")
            << " pts=" << frame->pts
            << " size=" << frame->width << "x" << frame->height;
        encodeLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    const int sendRet = avcodec_send_frame(codecContext(), frame);
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video)");
    }

    return receivePackets(context);
}

::media::Status VideoEncodeNode::stop(MediaGraphExecutionContext& context)
{
    if (hasCodecContext()) {
        if (auto status = drainEncoderForStop(); !status) {
            return status;
        }
    }
    m_encoderConfigEmitted = false;
    return FFmpegCodecNodeRuntime::stop(context);
}

::media::Status VideoEncodeNode::emitEncoderConfig(MediaGraphExecutionContext& context,
                                                   const MediaBufferRef& buffer)
{
    if (m_encoderConfigEmitted || !buffer) {
        return ::media::Status::success();
    }

    if (!context.findOutputChannel(nodeId(), "codec")) {
        m_encoderConfigEmitted = true;
        return ::media::Status::success();
    }

    auto status = emitOutput(context, "codec", buffer);
    if (!status) {
        return status;
    }

    m_encoderConfigEmitted = true;
    return ::media::Status::success();
}

::media::Status VideoEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = avcodec_receive_packet(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }

        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_packet(video)");
        }

        auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                                   "video_encode.packet");
        if (decision.shouldLog) {
            std::ostringstream out;
            out << "packet seq=" << decision.sequence
                << " codec=" << codecName(codecContext())
                << " pts=" << packet->pts
                << " dts=" << packet->dts
                << " duration=" << packet->duration
                << " size=" << packet->size;
            encodeLog(MediaGraphDiagnosticLevel::Flow, out.str());
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Video);
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        MediaTimeDescriptor timeDescriptor;
        timeDescriptor.timeBase = MediaRational{ codecContext()->time_base.num, codecContext()->time_base.den };
        buffer.value()->setTimeDescriptor(timeDescriptor);

        auto emitStatus = emitOutput(context, "packet", buffer.value());
        if (!emitStatus) {
            return emitStatus;
        }
    }
}

::media::Status VideoEncodeNode::drainEncoderForStop()
{
    const int sendRet = avcodec_send_frame(codecContext(), nullptr);
    if (sendRet < 0 && sendRet != AVERROR_EOF) {
        return FFmpegGraphError::statusFromCode(sendRet, "avcodec_send_frame(video stop)");
    }

    for (;;) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed("VideoEncodeNode stop failed: av_packet_alloc returned null"));
        }
        const int ret = avcodec_receive_packet(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return ::media::Status::success();
        }
        if (ret < 0) {
            return FFmpegGraphError::statusFromCode(ret, "avcodec_receive_packet(video stop)");
        }
    }
}

} // namespace media::ffmpeg::graph
