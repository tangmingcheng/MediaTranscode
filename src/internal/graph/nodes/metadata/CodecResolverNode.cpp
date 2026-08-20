#include "internal/graph/nodes/metadata/CodecResolverNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

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

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

::media::Result<bool> requiredBoolOption(const MediaNodeOptions* options,
                                         const std::string& key)
{
    const std::string value = optionValue(options, key);
    if (value == "1" || value == "true") {
        return ::media::Result<bool>::success(true);
    }
    if (value == "0" || value == "false") {
        return ::media::Result<bool>::success(false);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(
            "CodecResolverNode requires explicit boolean option: " + key));
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

::media::Result<MediaNodeProcessResult> CodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    auto* formatBuffer = dynamic_cast<FFmpegInputSnapshotBuffer*>(input.value()->get());
    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode expected complete input snapshots"));
    }

    const FFmpegInputStreamSnapshot* stream = nullptr;
    for (int index = 0; (stream = formatBuffer->inputStreamSnapshot(index)) != nullptr; ++index) {
        if (stream->streamKind == MediaStreamKind::Video) break;
    }
    if (!stream || stream->streamKind != MediaStreamKind::Video) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode requires video input snapshot"));
    }

    auto decoderStatus = resolveDecoder(context, *stream);
    if (!decoderStatus) {
        return processProgress(decoderStatus);
    }

    auto encoderStatus = resolveEncoder(context, *stream);
    if (!encoderStatus) {
        return processProgress(encoderStatus);
    }

    m_emitted = true;
    return processFinished();
}

::media::Status CodecResolverNode::resolveDecoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
{
    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) return ::media::Status::failure(codecParameters.error());
    const MediaNodeOptions* options = nodeOptions(context);
    const std::string plannedDecoder = optionValue(options, "decoder");
    auto hardwarePlannedResult = requiredBoolOption(options, "pipeline.hardware");
    if (!hardwarePlannedResult) {
        return ::media::Status::failure(hardwarePlannedResult.error());
    }
    const bool hardwarePlanned = hardwarePlannedResult.value();
    const std::string hwaccelName = optionValue(options, "pipeline.hwaccel");

    const AVCodec* decoder = nullptr;
    if (!plannedDecoder.empty() && plannedDecoder != "auto") {
        decoder = avcodec_find_decoder_by_name(plannedDecoder.c_str());
    } else {
        decoder = avcodec_find_decoder(codecParameters.value()->codec_id);
    }

    if (!decoder) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("CodecResolverNode failed: video decoder not found: " +
                                           (!plannedDecoder.empty() ? plannedDecoder : std::string(avcodec_get_name(codecParameters.value()->codec_id)))));
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: avcodec_alloc_context3(decoder) returned null"));
    }

    const int copyRet = avcodec_parameters_to_context(decoderContext.get(), codecParameters.value().get());
    if (copyRet < 0) {
        return FFmpegGraphError::statusFromCode(copyRet, "avcodec_parameters_to_context(video decoder)");
    }

    decoderContext->pkt_timebase = AVRational{ stream.time.timeBase.num, stream.time.timeBase.den };
    auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(
        options, "video.lineage.decoder_copy_opaque");
    if (!copyOpaque) {
        return ::media::Status::failure(copyOpaque.error());
    }
    if (copyOpaque.value()) {
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
        if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) {
            return status;
        }
        decoderContext->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
#else
        return requireMediaFfmpegCopyOpaqueCapability();
#endif
    }

    m_decoderHardwareDevice.reset();
    m_decoderHardwarePixelFormat = AV_PIX_FMT_NONE;
    bool decoderUsesHardwareDevice = false;
    bool decoderUsesHardwareFrames = false;
    if (hardwarePlanned) {
        const std::string plannedPixelFormat =
            optionValue(options, "decoder.output.pixel_format");
        m_decoderHardwarePixelFormat = av_get_pix_fmt(plannedPixelFormat.c_str());
        if (m_decoderHardwarePixelFormat == AV_PIX_FMT_NONE) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "CodecResolverNode requires planner-selected decoder output pixel format"));
        }
        auto requiresDeviceResult = requiredBoolOption(
            options, "decoder.output.requires_hw_device_ctx");
        if (!requiresDeviceResult) {
            return ::media::Status::failure(requiresDeviceResult.error());
        }
        auto requiresFramesResult = requiredBoolOption(
            options, "decoder.output.requires_hw_frames_ctx");
        if (!requiresFramesResult) {
            return ::media::Status::failure(requiresFramesResult.error());
        }
        const bool requiresDeviceContext = requiresDeviceResult.value();
        decoderUsesHardwareFrames = requiresFramesResult.value();
        const AVHWDeviceType plannedDeviceType = deviceTypeFromHwaccelName(hwaccelName);
        if (!ffmpegCodecSupportsPixelFormat(
                decoder,
                m_decoderHardwarePixelFormat,
                FFmpegCodecPixelFormatRequirement{
                    plannedDeviceType, true, requiresDeviceContext})) {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported(
                    "CodecResolverNode selected decoder does not advertise the planned hardware frame contract"));
        }

        decoderUsesHardwareDevice = requiresDeviceContext;

        if (requiresDeviceContext) {
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
        << " hw_frames_contract=" << (decoderUsesHardwareFrames ? "required" : "internal")
        << " pkt_tb=" << stream.time.timeBase.num << "/" << stream.time.timeBase.den;
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

::media::Status CodecResolverNode::resolveEncoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
{
    const MediaNodeOptions* options = nodeOptions(context);

    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) return ::media::Status::failure(codecParameters.error());

    CodecResolverEncoderContextBuildRequest request;
    request.codecParameters = codecParameters.value().get();
    request.sourceFormat = stream.format;
    request.sourceTime = stream.time;
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
