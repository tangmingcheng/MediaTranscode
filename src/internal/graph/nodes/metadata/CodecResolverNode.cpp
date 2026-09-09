#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"

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

::media::Status CodecResolverNode::bindPreparedEncoder(MediaBufferRef encoder)
{
    auto* codec = dynamic_cast<FFmpegCodecContextBuffer*>(encoder.get());
    if (m_emitted || m_preparedEncoder || !codec || !codec->context() ||
        !avcodec_is_open(codec->context())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Output branch requires one opened encoder before runtime start"));
    }
    auto readback = MediaVideoEncoderReadback::capture(*codec->context());
    if (!readback) return ::media::Status::failure(readback.error());
    {
        std::lock_guard lock(m_snapshotMutex);
        m_encoderReadback = std::move(readback).value();
    }
    m_preparedEncoder = std::move(encoder);
    return ::media::Status::success();
}

::media::Result<MediaVideoEncoderReadback> CodecResolverNode::encoderReadback() const
{
    std::lock_guard lock(m_snapshotMutex);
    if (!m_encoderReadback) return ::media::Result<MediaVideoEncoderReadback>::failure(
        ::media::ErrorInfo::notInitialized("encoder has not published immutable opened readback"));
    return ::media::Result<MediaVideoEncoderReadback>::success(*m_encoderReadback);
}

MediaBufferRef CodecResolverNode::inputSnapshot() const
{
    std::lock_guard lock(m_snapshotMutex);
    return m_inputSnapshot;
}

MediaBufferRef CodecResolverNode::timestampSource() const
{
    std::lock_guard lock(m_snapshotMutex);
    return m_timestampSource;
}

::media::Result<MediaDecoderRuntimeFacts> CodecResolverNode::decoderRuntimeFacts() const
{
    std::lock_guard lock(m_snapshotMutex);
    if (!m_decoderRuntimeFacts) return ::media::Result<MediaDecoderRuntimeFacts>::failure(
        ::media::ErrorInfo::notInitialized("Shared decoder has not published opened codec facts"));
    return ::media::Result<MediaDecoderRuntimeFacts>::success(*m_decoderRuntimeFacts);
}

::media::Result<MediaNodeProcessResult> CodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    if (nodeOption(context, "codec_resolver.mode") == "output_branch") {
        if (!m_preparedEncoder) {
            return processProgress(::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Output branch encoder was not prepared before publication")));
        }
        auto status = emitOutput(context, "encoder", m_preparedEncoder);
        if (!status) return processProgress(std::move(status));
        m_preparedEncoder.reset();
        m_emitted = true;
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

    {
        std::lock_guard lock(m_snapshotMutex);
        m_inputSnapshot = *input.value();
    }

    auto decoderStatus = prepareDecoder(context, *stream);
    if (!decoderStatus) {
        return processProgress(decoderStatus);
    }

    auto encoderStatus = prepareEncoder(context, *stream);
    if (!encoderStatus) {
        return processProgress(encoderStatus);
    }

    // Prepare both codecs before publishing either. The decoder port releases
    // live frames, so publish its configuration only after all target timing
    // metadata is available to downstream workers.
    auto encoderPublished = emitOutput(context, "encoder", m_preparedEncoder);
    if (!encoderPublished) return processProgress(encoderPublished);
    m_preparedEncoder.reset();
    if (auto timestamp = timestampSource()) {
        auto timingPublished = emitOutput(context, "timestamp_source", std::move(timestamp));
        if (!timingPublished) return processProgress(timingPublished);
    }
    auto decoderPublished = emitOutput(context, "decoder", m_preparedDecoder);
    if (!decoderPublished) return processProgress(decoderPublished);
    m_preparedDecoder.reset();
    m_emitted = true;
    return processFinished();
}

::media::Status CodecResolverNode::prepareDecoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
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

    const bool hasInputRetention = options && options->has(
        "decoder.pipeline.input_retention.maximum_internal_packets");
    if (hasInputRetention) {
        auto count = requiredPositiveIntNodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.thread_count");
        auto type = requiredNonNegativeIntNodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.thread_type");
        if (!count || !type) return ::media::Status::failure(!count ? count.error() : type.error());
        decoderContext->thread_count = count.value();
        decoderContext->thread_type = type.value();
    }
    const int openRet = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (openRet < 0) {
        return FFmpegGraphError::statusFromCode(openRet, "avcodec_open2(video decoder)");
    }

    if (hasInputRetention) {
        auto planned = requiredPositiveInt64NodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.maximum_internal_packets");
        auto observed = MediaDecoderInputRetentionAdapter::readAfterOpen(*decoderContext, hwaccelName);
        if (!planned) return ::media::Status::failure(planned.error());
        if (!observed || observed->maximumInternalPackets() >
            static_cast<std::uint64_t>(planned.value())) {
            return ::media::Status::failure(::media::ErrorInfo::unsupported(
                "opened decoder input retention exceeds its prepared allocation contract"));
        }
    }
    {
        std::lock_guard lock(m_snapshotMutex);
        m_decoderRuntimeFacts = MediaDecoderRuntimeFacts{
            decoder->name, decoderContext->hwaccel_flags};
    }
    codecResolverLog(MediaGraphDiagnosticLevel::State,
        std::string("decoder.runtime name=") + decoder->name +
        " hwaccel_flags=" + std::to_string(decoderContext->hwaccel_flags));

    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoderContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    m_preparedDecoder = std::move(buffer).value();

    if (context.findOutputChannel(nodeId(), "timestamp_source")) {
        auto timestampContext = ::media::ffmpeg::makeCodecContext(nullptr);
        if (!timestampContext) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
                "Could not allocate immutable decoder timing metadata"));
        }
        timestampContext->pkt_timebase =
            AVRational{stream.time.timeBase.num, stream.time.timeBase.den};
        timestampContext->time_base = timestampContext->pkt_timebase;
        timestampContext->codec_type = AVMEDIA_TYPE_VIDEO;
        auto timestampBuffer = FFmpegBufferFactory::wrapCodecContext(
            std::move(timestampContext));
        if (!timestampBuffer) return ::media::Status::failure(timestampBuffer.error());
        {
            std::lock_guard lock(m_snapshotMutex);
            m_timestampSource = timestampBuffer.value();
        }
    }

    return ::media::Status::success();
}

::media::Status CodecResolverNode::prepareEncoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream)
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

    auto readback = MediaVideoEncoderReadback::capture(*encoderContext);
    if (!readback) return ::media::Status::failure(readback.error());
    {
        std::lock_guard lock(m_snapshotMutex);
        m_encoderReadback = std::move(readback).value();
    }
    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(encoderBuild.context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    m_preparedEncoder = std::move(buffer).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
