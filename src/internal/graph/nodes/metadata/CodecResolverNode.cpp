#include "internal/graph/nodes/metadata/CodecResolverNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
namespace {

struct HardwareDecoderSelection {
    AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
    AVHWDeviceType deviceType = AV_HWDEVICE_TYPE_NONE;
    bool requiresDeviceContext = false;
};

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

bool truthyOption(const MediaNodeOptions* options, const std::string& key)
{
    const std::string value = optionValue(options, key);
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string("unknown");
}

AVHWDeviceType deviceTypeFromHwaccelName(const std::string& hwaccel)
{
    if (hwaccel.empty()) {
        return AV_HWDEVICE_TYPE_NONE;
    }
    if (hwaccel == "rkmpp") {
        return AV_HWDEVICE_TYPE_DRM;
    }
    return av_hwdevice_find_type_by_name(hwaccel.c_str());
}

HardwareDecoderSelection selectHardwareDecoder(const AVCodec* decoder, AVHWDeviceType deviceType)
{
    HardwareDecoderSelection internalSelection;
    if (!decoder) {
        return {};
    }

    for (int index = 0;; ++index) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, index);
        if (!config) {
            break;
        }

        if (config->pix_fmt == AV_PIX_FMT_NONE) {
            continue;
        }

        const bool canUseDevice = (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
        const bool internalOnly = (config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) != 0;
        if (canUseDevice && config->device_type == deviceType) {
            return HardwareDecoderSelection{ config->pix_fmt, deviceType, true };
        }

        if (internalOnly && internalSelection.pixelFormat == AV_PIX_FMT_NONE) {
            internalSelection = HardwareDecoderSelection{ config->pix_fmt, config->device_type, false };
        }
    }

    return internalSelection;
}

AVPixelFormat plannedHardwareGetFormat(AVCodecContext* context, const AVPixelFormat* formats)
{
    const auto* desired = static_cast<const AVPixelFormat*>(context ? context->opaque : nullptr);
    if (!desired || *desired == AV_PIX_FMT_NONE) {
        return formats ? formats[0] : AV_PIX_FMT_NONE;
    }

    for (const AVPixelFormat* current = formats; current && *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == *desired) {
            return *current;
        }
    }

    return AV_PIX_FMT_NONE;
}

void codecResolverLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("codec_resolver.") + message);
}

} // namespace

CodecResolverNode::CodecResolverNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "CodecResolverNode")
{
}

MediaNodeKind CodecResolverNode::staticKind() noexcept
{
    return MediaNodeKind::CodecResolver;
}

::media::Status CodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return ::media::Status::success();
    }

    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value().get());
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode expected FFmpegFormatContextBuffer"));
    }

    AVFormatContext* formatContext = formatBuffer->context();

    auto decoderStatus = resolveDecoder(context, formatContext);
    if (!decoderStatus) {
        return decoderStatus;
    }

    auto encoderStatus = resolveEncoder(context, formatContext);
    if (!encoderStatus) {
        return encoderStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

::media::Status CodecResolverNode::resolveDecoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext)
{
    const int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return FFmpegGraphError::statusFromCode(streamIndex, "av_find_best_stream(video decoder)");
    }

    AVStream* stream = formatContext->streams[streamIndex];
    const MediaNodeOptions* options = nodeOptions(context);
    const std::string plannedDecoder = optionValue(options, "decoder");
    const bool hardwarePlanned = truthyOption(options, "pipeline.hardware");
    const std::string hwaccelName = optionValue(options, "pipeline.hwaccel");

    const AVCodec* decoder = nullptr;
    if (!plannedDecoder.empty() && plannedDecoder != "auto") {
        decoder = avcodec_find_decoder_by_name(plannedDecoder.c_str());
    } else {
        decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    }

    if (!decoder) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("CodecResolverNode failed: video decoder not found: " +
                                           (!plannedDecoder.empty() ? plannedDecoder : std::string(avcodec_get_name(stream->codecpar->codec_id)))));
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: avcodec_alloc_context3(decoder) returned null"));
    }

    const int copyRet = avcodec_parameters_to_context(decoderContext.get(), stream->codecpar);
    if (copyRet < 0) {
        return FFmpegGraphError::statusFromCode(copyRet, "avcodec_parameters_to_context(video decoder)");
    }

    decoderContext->pkt_timebase = stream->time_base;

    m_decoderHardwareDevice.reset();
    m_decoderHardwarePixelFormat = AV_PIX_FMT_NONE;
    bool decoderUsesHardwareDevice = false;
    if (hardwarePlanned) {
        const AVHWDeviceType plannedDeviceType = deviceTypeFromHwaccelName(hwaccelName);
        HardwareDecoderSelection hardwareSelection = selectHardwareDecoder(decoder, plannedDeviceType);
        if (hardwareSelection.pixelFormat == AV_PIX_FMT_NONE) {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported("CodecResolverNode planned hardware decoder format is not supported by selected decoder"));
        }

        m_decoderHardwarePixelFormat = hardwareSelection.pixelFormat;
        decoderUsesHardwareDevice = hardwareSelection.requiresDeviceContext;

        if (hardwareSelection.requiresDeviceContext) {
            if (plannedDeviceType == AV_HWDEVICE_TYPE_NONE) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("CodecResolverNode planned hardware decoder requires valid pipeline.hwaccel"));
            }

            AVBufferRef* rawDevice = nullptr;
            const int deviceRet = av_hwdevice_ctx_create(&rawDevice, plannedDeviceType, nullptr, nullptr, 0);
            if (deviceRet < 0) {
                return FFmpegGraphError::statusFromCode(deviceRet, "av_hwdevice_ctx_create(" + hwaccelName + ")");
            }
            m_decoderHardwareDevice = ::media::ffmpeg::BufferRefPtr(rawDevice);
            decoderContext->hw_device_ctx = av_buffer_ref(m_decoderHardwareDevice.get());
            if (!decoderContext->hw_device_ctx) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: av_buffer_ref(hw_device_ctx)"));
            }
        }

        decoderContext->opaque = &m_decoderHardwarePixelFormat;
        decoderContext->get_format = plannedHardwareGetFormat;
    }

    std::ostringstream out;
    out << "decoder.open name=" << (decoder->name ? decoder->name : "unknown")
        << " planned=" << (plannedDecoder.empty() ? "auto" : plannedDecoder)
        << " hardware=" << (hardwarePlanned ? "true" : "false")
        << " hwaccel=" << (hwaccelName.empty() ? "none" : hwaccelName)
        << " hw_pix_fmt=" << pixelFormatName(m_decoderHardwarePixelFormat)
        << " hw_device_ctx=" << (decoderUsesHardwareDevice ? "set" : "none")
        << " pkt_tb=" << stream->time_base.num << "/" << stream->time_base.den;
    codecResolverLog(MediaGraphDiagnosticLevel::State, out.str());

    const int openRet = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (openRet < 0) {
        return FFmpegGraphError::statusFromCode(openRet, "avcodec_open2(video decoder)");
    }

    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoderContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    MediaBufferRef decoderConfig = std::move(buffer).value();
    auto decoderStatus = emitOutput(context, "decoder", decoderConfig);
    if (!decoderStatus) {
        return decoderStatus;
    }

    if (context.findOutputChannel(nodeId(), "timestamp_source")) {
        auto timestampStatus = emitOutput(context, "timestamp_source", decoderConfig);
        if (!timestampStatus) {
            return timestampStatus;
        }
    }

    return ::media::Status::success();
}

::media::Status CodecResolverNode::resolveEncoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext)
{
    const int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return FFmpegGraphError::statusFromCode(streamIndex, "av_find_best_stream(video encoder source)");
    }

    AVStream* stream = formatContext->streams[streamIndex];
    const MediaNodeOptions* options = nodeOptions(context);

    CodecResolverEncoderContextBuildRequest request;
    request.formatContext = formatContext;
    request.stream = stream;
    request.options = options;
    request.hardwareDevice = m_decoderHardwareDevice.get();

    auto encoderBuildResult = CodecResolverEncoderContextBuilder::build(request);
    if (!encoderBuildResult) {
        return ::media::Status::failure(encoderBuildResult.error());
    }

    CodecResolverEncoderContextBuildResult encoderBuild = std::move(encoderBuildResult).value();
    AVCodecContext* encoderContext = encoderBuild.context.get();
    const AVCodec* encoder = encoderContext ? encoderContext->codec : nullptr;

    codecResolverLog(MediaGraphDiagnosticLevel::State,
                     std::string("encoder.open name=") +
                         (encoder && encoder->name ? encoder->name : optionValue(options, "encoder", "unknown")) +
                         " pix_fmt=" + pixelFormatName(encoderContext ? encoderContext->pix_fmt : AV_PIX_FMT_NONE) +
                         " hw_frames_format=" + pixelFormatName(encoderBuild.hardwareFramesFormat) +
                         " surface_sw_format=" + pixelFormatName(encoderBuild.surfaceSoftwareFormat) +
                         " frame_kind=" + optionValue(options, "encoder.pipeline.frame_kind", "missing") +
                         " hwaccel=" + optionValue(options, "encoder.pipeline.hwaccel", "missing") +
                         " hw_device_ctx=" + (encoderContext && encoderContext->hw_device_ctx ? "set" : "none") +
                         " hw_frames_ctx=" + (encoderContext && encoderContext->hw_frames_ctx ? "set" : "none"));

    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(encoderBuild.context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    return emitOutput(context, "encoder", std::move(buffer).value());
}

} // namespace media::ffmpeg::graph
