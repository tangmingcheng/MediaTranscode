#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

#include <new>
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

struct DecoderFormatCallbackState final {
    const AVPixelFormat pixelFormat;
};

AVPixelFormat plannedHardwareGetFormat(AVCodecContext* context, const AVPixelFormat* formats)
{
    const auto* desired = static_cast<const DecoderFormatCallbackState*>(context ? context->opaque : nullptr);
    if (!desired || desired->pixelFormat == AV_PIX_FMT_NONE) {
        return AV_PIX_FMT_NONE;
    }

    for (const AVPixelFormat* current = formats; current && *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == desired->pixelFormat) {
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

::media::Result<CodecResolverDecoderContextBuildResult>
CodecResolverDecoderContextBuilder::build(const CodecResolverDecoderContextBuildRequest& request)
{
    using Result = ::media::Result<CodecResolverDecoderContextBuildResult>;
    if (!request.codecParameters)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder context builder requires codec parameters"));
    const MediaNodeOptions* options = request.options;
    const std::string plannedDecoder = optionValue(options, "decoder");
    auto hardwarePlannedResult = requiredBoolOption(options, "pipeline.hardware");
    if (!hardwarePlannedResult) {
        return Result::failure(hardwarePlannedResult.error());
    }
    const bool hardwarePlanned = hardwarePlannedResult.value();
    const std::string hwaccelName = optionValue(options, "pipeline.hwaccel");

    const AVCodec* decoder = nullptr;
    if (!plannedDecoder.empty() && plannedDecoder != "auto") {
        decoder = avcodec_find_decoder_by_name(plannedDecoder.c_str());
    } else {
        decoder = avcodec_find_decoder(request.codecParameters->codec_id);
    }

    if (!decoder) {
        return Result::failure(
            ::media::ErrorInfo::unsupported("CodecResolverNode failed: video decoder not found: " +
                                           (!plannedDecoder.empty() ? plannedDecoder : std::string(avcodec_get_name(request.codecParameters->codec_id)))));
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return Result::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: avcodec_alloc_context3(decoder) returned null"));
    }

    const int copyRet = avcodec_parameters_to_context(decoderContext.get(), request.codecParameters);
    if (copyRet < 0) {
        return Result::failure(FFmpegGraphError::fromCode(copyRet, "avcodec_parameters_to_context(video decoder)"));
    }

    decoderContext->pkt_timebase = AVRational{ request.sourceTime.timeBase.num, request.sourceTime.timeBase.den };
    auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(
        options, "video.lineage.decoder_copy_opaque");
    if (!copyOpaque) {
        return Result::failure(copyOpaque.error());
    }
    if (copyOpaque.value()) {
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
        if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) {
            return Result::failure(status.error());
        }
        decoderContext->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
#else
        return Result::failure(requireMediaFfmpegCopyOpaqueCapability().error());
#endif
    }

    ::media::ffmpeg::BufferRefPtr hardwareDevice;
    AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;
    bool decoderUsesHardwareDevice = false;
    bool decoderUsesHardwareFrames = false;
    if (hardwarePlanned) {
        const std::string plannedPixelFormat =
            optionValue(options, "decoder.output.pixel_format");
        hardwarePixelFormat = av_get_pix_fmt(plannedPixelFormat.c_str());
        if (hardwarePixelFormat == AV_PIX_FMT_NONE) {
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(
                    "CodecResolverNode requires planner-selected decoder output pixel format"));
        }
        auto requiresDeviceResult = requiredBoolOption(
            options, "decoder.output.requires_hw_device_ctx");
        if (!requiresDeviceResult) {
            return Result::failure(requiresDeviceResult.error());
        }
        auto requiresFramesResult = requiredBoolOption(
            options, "decoder.output.requires_hw_frames_ctx");
        if (!requiresFramesResult) {
            return Result::failure(requiresFramesResult.error());
        }
        const bool requiresDeviceContext = requiresDeviceResult.value();
        decoderUsesHardwareFrames = requiresFramesResult.value();
        const AVHWDeviceType plannedDeviceType = deviceTypeFromHwaccelName(hwaccelName);
        if (!ffmpegCodecSupportsPixelFormat(
                decoder,
                hardwarePixelFormat,
                FFmpegCodecPixelFormatRequirement{
                    plannedDeviceType, true, requiresDeviceContext})) {
            return Result::failure(
                ::media::ErrorInfo::unsupported(
                    "CodecResolverNode selected decoder does not advertise the planned hardware frame contract"));
        }

        decoderUsesHardwareDevice = requiresDeviceContext;

        if (requiresDeviceContext) {
            if (plannedDeviceType == AV_HWDEVICE_TYPE_NONE) {
                return Result::failure(
                    ::media::ErrorInfo::invalidArgument("CodecResolverNode planned hardware decoder requires valid pipeline.hwaccel"));
            }

            if (request.hardwareDevice) {
                if (!request.hardwareDevice->data)
                    return Result::failure(::media::ErrorInfo::invalidArgument(
                        "Prepared decoder device has no context"));
                const auto* device = reinterpret_cast<const AVHWDeviceContext*>(request.hardwareDevice->data);
                if (device->type != plannedDeviceType)
                    return Result::failure(::media::ErrorInfo::invalidArgument(
                        "Prepared decoder device differs from the selected hardware contract"));
                hardwareDevice.reset(av_buffer_ref(request.hardwareDevice));
                if (!hardwareDevice)
                    return Result::failure(::media::ErrorInfo::allocationFailed(
                        "Could not retain the shared decoder hardware device"));
            } else {
                if (optionValue(options, "codec_resolver.mode") == "source_decode")
                    return Result::failure(::media::ErrorInfo::notInitialized(
                        "Composition source decoder requires its prepared shared device"));
                AVBufferRef* rawDevice = nullptr;
                const int deviceRet = av_hwdevice_ctx_create(&rawDevice, plannedDeviceType, nullptr, nullptr, 0);
                if (deviceRet < 0) {
                    return Result::failure(FFmpegGraphError::fromCode(deviceRet, "av_hwdevice_ctx_create(" + hwaccelName + ")"));
                }
                hardwareDevice = ::media::ffmpeg::BufferRefPtr(rawDevice);
            }
            decoderContext->hw_device_ctx = av_buffer_ref(hardwareDevice.get());
            if (!decoderContext->hw_device_ctx) {
                return Result::failure(
                    ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: av_buffer_ref(hw_device_ctx)"));
            }
        }

        std::shared_ptr<DecoderFormatCallbackState> callbackState;
        try {
            callbackState = std::make_shared<DecoderFormatCallbackState>(DecoderFormatCallbackState{hardwarePixelFormat});
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "Could not allocate decoder format callback owner"));
        }
        decoderContext.get_deleter().callbackOwner = callbackState;
        decoderContext->opaque = callbackState.get();
        decoderContext->get_format = plannedHardwareGetFormat;
    }

    std::ostringstream out;
    out << "decoder.open name=" << (decoder->name ? decoder->name : "unknown")
        << " planned=" << (plannedDecoder.empty() ? "auto" : plannedDecoder)
        << " hardware=" << (hardwarePlanned ? "true" : "false")
        << " hwaccel=" << (hwaccelName.empty() ? "none" : hwaccelName)
        << " hw_pix_fmt=" << pixelFormatName(hardwarePixelFormat)
        << " hw_device_ctx=" << (decoderUsesHardwareDevice ? "set" : "none")
        << " hw_frames_contract=" << (decoderUsesHardwareFrames ? "required" : "internal")
        << " pkt_tb=" << request.sourceTime.timeBase.num << "/" << request.sourceTime.timeBase.den;
    codecResolverLog(MediaGraphDiagnosticLevel::State, out.str());

    const bool hasInputRetention = options && options->has(
        "decoder.pipeline.input_retention.maximum_internal_packets");
    if (hasInputRetention) {
        auto count = requiredPositiveIntNodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.thread_count");
        auto type = requiredNonNegativeIntNodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.thread_type");
        if (!count || !type) return Result::failure(!count ? count.error() : type.error());
        decoderContext->thread_count = count.value();
        decoderContext->thread_type = type.value();
    }
    const int openRet = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (openRet < 0) {
        return Result::failure(FFmpegGraphError::fromCode(openRet, "avcodec_open2(video decoder)"));
    }

    if (hasInputRetention) {
        auto planned = requiredPositiveInt64NodeOption(options, "CodecResolverNode",
            "decoder.pipeline.input_retention.maximum_internal_packets");
        auto observed = MediaDecoderInputRetentionAdapter::readAfterOpen(*decoderContext, hwaccelName);
        if (!planned) return Result::failure(planned.error());
        if (!observed || observed->maximumInternalPackets() >
            static_cast<std::uint64_t>(planned.value())) {
            return Result::failure(::media::ErrorInfo::unsupported(
                "opened decoder input retention exceeds its prepared allocation contract"));
        }
    }
    MediaDecoderRuntimeFacts facts{decoder->name, decoderContext->hwaccel_flags};
    codecResolverLog(MediaGraphDiagnosticLevel::State,
        std::string("decoder.runtime name=") + decoder->name +
        " hwaccel_flags=" + std::to_string(decoderContext->hwaccel_flags));
    return Result::success({std::move(decoderContext), std::move(hardwareDevice), std::move(facts)});
}

} // namespace media::ffmpeg::graph
