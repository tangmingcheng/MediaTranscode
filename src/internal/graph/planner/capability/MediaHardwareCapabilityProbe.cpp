#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaHardwareCapability unavailable(std::string reason)
{
    return {false, std::move(reason)};
}

MediaHardwareCapability ffmpegUnavailable(const std::string& operation, int code)
{
    return unavailable(operation + " failed: " + FFmpegGraphError::describe(code));
}

bool rkmppRuntimeAvailable()
{
    return MediaHardwareCapabilityProbe::decoderExists("h264_rkmpp") ||
           MediaHardwareCapabilityProbe::decoderExists("hevc_rkmpp") ||
           MediaHardwareCapabilityProbe::encoderExists("h264_rkmpp") ||
           MediaHardwareCapabilityProbe::encoderExists("hevc_rkmpp");
}

AVPixelFormat pixelFormat(const std::string& name) noexcept
{
    return name.empty() ? AV_PIX_FMT_NONE : av_get_pix_fmt(name.c_str());
}

bool decoderSupportsDevice(const AVCodec& decoder,
                           AVHWDeviceType deviceType,
                           AVPixelFormat hardwareFormat) noexcept
{
    for (int index = 0;; ++index) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(&decoder, index);
        if (!config) {
            return false;
        }
        const bool deviceContextSupported =
            (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
        if (config->device_type == deviceType && deviceContextSupported &&
            (hardwareFormat == AV_PIX_FMT_NONE || config->pix_fmt == hardwareFormat)) {
            return true;
        }
    }
}

::media::ffmpeg::BufferRefPtr createFramesContext(
    AVBufferRef* device,
    AVPixelFormat hardwareFormat,
    AVPixelFormat softwareFormat,
    int width,
    int height,
    int initialPoolSize,
    std::string& failure)
{
    AVBufferRef* rawFrames = av_hwframe_ctx_alloc(device);
    ::media::ffmpeg::BufferRefPtr frames(rawFrames);
    if (!frames) {
        failure = "av_hwframe_ctx_alloc returned null";
        return {};
    }

    auto* context = reinterpret_cast<AVHWFramesContext*>(frames->data);
    context->format = hardwareFormat;
    context->sw_format = softwareFormat;
    context->width = width;
    context->height = height;
    context->initial_pool_size = initialPoolSize;
    const int result = av_hwframe_ctx_init(frames.get());
    if (result < 0) {
        failure = "av_hwframe_ctx_init failed: " + FFmpegGraphError::describe(result);
        return {};
    }
    return frames;
}

MediaHardwareCapability validateCompleteChain(
    const MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    if (!chain.allHardware || !chain.sameHardwareDevice) {
        return unavailable(
            "hardware chain validation requires one hardware device across all active stages");
    }
    if (options.probeWidth <= 0 || options.probeHeight <= 0) {
        return unavailable("hardware chain validation requires planner-resolved probe dimensions");
    }
    if (!options.probeFrameRate.isKnown()) {
        return unavailable("hardware chain validation requires planner-resolved probe frame rate");
    }

    const int outputWidth = options.targetWidth > 0 ? options.targetWidth : options.probeWidth;
    const int outputHeight = options.targetHeight > 0 ? options.targetHeight : options.probeHeight;
    const AVPixelFormat hardwareFormat = pixelFormat(chain.encoder.hardwareFramesFormat);
    const AVPixelFormat surfaceFormat = pixelFormat(chain.encoder.surfacePixelFormat);
    const AVPixelFormat encoderFormat = pixelFormat(chain.encoder.pixelFormat);
    if (encoderFormat == AV_PIX_FMT_NONE) {
        return unavailable("planned encoder pixel format is not recognized by FFmpeg");
    }

    AVHWDeviceType deviceType = AV_HWDEVICE_TYPE_NONE;
    ::media::ffmpeg::BufferRefPtr device;
    if (chain.decoder.deviceKind == MediaHardwareDeviceKind::RKMPP) {
        if (!rkmppRuntimeAvailable()) {
            return unavailable("RKMPP runtime codecs are unavailable");
        }
    } else {
        if (chain.decoder.hwaccelName.empty()) {
            return unavailable("hardware chain has no planned FFmpeg hardware device name");
        }
        deviceType = av_hwdevice_find_type_by_name(chain.decoder.hwaccelName.c_str());
        if (deviceType == AV_HWDEVICE_TYPE_NONE) {
            return unavailable("hardware backend is not recognized by FFmpeg");
        }

        AVBufferRef* rawDevice = nullptr;
        const int created = av_hwdevice_ctx_create(
            &rawDevice, deviceType, nullptr, nullptr, 0);
        device.reset(rawDevice);
        if (created < 0 || !device) {
            return ffmpegUnavailable("av_hwdevice_ctx_create", created);
        }
    }

    const AVCodec* decoder =
        avcodec_find_decoder_by_name(chain.decoder.ffmpegName.c_str());
    if (!decoder) {
        return unavailable("planned decoder is unavailable: " + chain.decoder.ffmpegName);
    }
    if (device &&
        !decoderSupportsDevice(*decoder, deviceType, hardwareFormat)) {
        return unavailable(
            "planned decoder does not expose the required hardware device/pixel-format config");
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return unavailable("avcodec_alloc_context3(decoder) returned null");
    }
    decoderContext->width = options.probeWidth;
    decoderContext->height = options.probeHeight;
    if (device) {
        decoderContext->hw_device_ctx = av_buffer_ref(device.get());
        if (!decoderContext->hw_device_ctx) {
            return unavailable("av_buffer_ref(decoder hardware device) returned null");
        }
    }
    const int decoderOpened = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (decoderOpened < 0) {
        return ffmpegUnavailable(
            "avcodec_open2(decoder " + chain.decoder.ffmpegName + ")",
            decoderOpened);
    }

    ::media::ffmpeg::BufferRefPtr sourceFrames;
    if (device) {
        if (hardwareFormat == AV_PIX_FMT_NONE || surfaceFormat == AV_PIX_FMT_NONE) {
            return unavailable(
                "hardware chain requires planned hardware and surface pixel formats");
        }
        std::string framesFailure;
        sourceFrames = createFramesContext(
            device.get(), hardwareFormat, surfaceFormat,
            options.probeWidth, options.probeHeight, 4, framesFailure);
        if (!sourceFrames) {
            return unavailable("decoder/filter frame negotiation " + framesFailure);
        }
    }

    const AVCodec* encoder =
        avcodec_find_encoder_by_name(chain.encoder.ffmpegName.c_str());
    if (!encoder) {
        return unavailable("planned encoder is unavailable: " + chain.encoder.ffmpegName);
    }
    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return unavailable("avcodec_alloc_context3(encoder) returned null");
    }
    encoderContext->width = outputWidth;
    encoderContext->height = outputHeight;
    encoderContext->pix_fmt = encoderFormat;
    encoderContext->time_base =
        AVRational{options.probeFrameRate.den, options.probeFrameRate.num};
    encoderContext->framerate =
        AVRational{options.probeFrameRate.num, options.probeFrameRate.den};
    encoderContext->sample_aspect_ratio = AVRational{1, 1};
    if (options.lowLatency) {
        encoderContext->max_b_frames = 0;
    }

    if (device) {
        encoderContext->hw_device_ctx = av_buffer_ref(device.get());
        if (!encoderContext->hw_device_ctx) {
            return unavailable("av_buffer_ref(encoder hardware device) returned null");
        }
        std::string framesFailure;
        auto encoderFrames = createFramesContext(
            device.get(), hardwareFormat, surfaceFormat,
            outputWidth, outputHeight, 4, framesFailure);
        if (!encoderFrames) {
            return unavailable("filter/encoder frame negotiation " + framesFailure);
        }
        encoderContext->hw_frames_ctx = encoderFrames.release();
    }

    if (options.filterRequired) {
        auto firstFrame = ::media::ffmpeg::makeFrame();
        if (!firstFrame) {
            return unavailable("av_frame_alloc(filter capability) returned null");
        }
        firstFrame->format = device ? hardwareFormat : encoderFormat;
        firstFrame->width = options.probeWidth;
        firstFrame->height = options.probeHeight;
        if (sourceFrames) {
            firstFrame->hw_frames_ctx = av_buffer_ref(sourceFrames.get());
            if (!firstFrame->hw_frames_ctx) {
                return unavailable("av_buffer_ref(filter source frames) returned null");
            }
        }

        MediaNodeOptions filterOptions;
        filterOptions.set("filter.pipeline.filter", chain.filter.filterName);
        VideoFilterGraphBuildRequest request;
        request.options = &filterOptions;
        request.firstFrame = firstFrame.get();
        request.inputTimeBase =
            AVRational{options.probeFrameRate.den, options.probeFrameRate.num};
        request.inputFrameRate =
            AVRational{options.probeFrameRate.num, options.probeFrameRate.den};
        request.sampleAspectRatio = AVRational{1, 1};
        auto filterGraph = VideoFilterGraphBuilder::build(request);
        if (!filterGraph) {
            return unavailable(
                "filter graph negotiation failed: " + filterGraph.error().message);
        }
        const int negotiatedFormat =
            av_buffersink_get_format(filterGraph.value().bufferSink);
        const int negotiatedWidth =
            av_buffersink_get_w(filterGraph.value().bufferSink);
        const int negotiatedHeight =
            av_buffersink_get_h(filterGraph.value().bufferSink);
        if (negotiatedFormat != encoderContext->pix_fmt ||
            negotiatedWidth != outputWidth || negotiatedHeight != outputHeight) {
            return unavailable(
                "filter output does not negotiate the planned encoder format and dimensions");
        }
    }

    const int encoderOpened = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (encoderOpened < 0) {
        return ffmpegUnavailable(
            "avcodec_open2(encoder " + chain.encoder.ffmpegName + ")",
            encoderOpened);
    }

    return {true, "decoder/filter/encoder chain opened and negotiated"};
}

} // namespace

bool MediaHardwareCapabilityProbe::decoderExists(const std::string& name) noexcept
{
    return !name.empty() && avcodec_find_decoder_by_name(name.c_str()) != nullptr;
}

bool MediaHardwareCapabilityProbe::encoderExists(const std::string& name) noexcept
{
    return !name.empty() && avcodec_find_encoder_by_name(name.c_str()) != nullptr;
}

bool MediaHardwareCapabilityProbe::filterExists(const std::string& name) noexcept
{
    return !name.empty() && avfilter_get_by_name(name.c_str()) != nullptr;
}

MediaHardwareCapabilityProbe::MediaHardwareCapabilityProbe()
    : m_chainValidator(validateCompleteChain)
{
}

MediaHardwareCapabilityProbe::MediaHardwareCapabilityProbe(
    ChainValidator chainValidator)
    : m_chainValidator(std::move(chainValidator))
{
}

::media::Status MediaHardwareCapabilityProbe::validate(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options) const
{
    if (!m_chainValidator) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "hardware capability probe requires a complete-chain validator"));
    }

    MediaHardwareCapability capability = m_chainValidator(chain, options);
    std::ostringstream out;
    out << "backend=" << mediaHardwareDeviceKindName(chain.decoder.deviceKind)
        << " status=" << (capability.available ? "found" : "not_found")
        << " probe=decoder_filter_encoder_open"
        << " note=" << capability.reason;
    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::PlannerCapability,
                            out.str());

    chain.decoder.available = capability.available;
    chain.decoder.availabilityReason = capability.reason;
    if (options.filterRequired) {
        chain.filter.available = capability.available;
        chain.filter.availabilityReason = capability.reason;
    }
    chain.encoder.available = capability.available;
    chain.encoder.availabilityReason = capability.reason;
    if (!capability.available) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "hardware candidate " + chain.label + " failed validation: " +
                capability.reason));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
