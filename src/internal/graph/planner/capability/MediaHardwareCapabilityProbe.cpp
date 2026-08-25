#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
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

::media::Status publishPacketLayout(
    MediaPipelineChainPlan& chain, AVCodecContext& context)
{
    auto layout =
        MediaEncoderPacketLayoutCapabilityProvider::probeOpenedContext(context);
    if (!layout) return ::media::Status::failure(layout.error());
    chain.encoder.encodedPacketLayout = std::move(layout).value();
    return ::media::Status::success();
}

::media::Status publishPacketLayoutThroughAdvertisedSoftwareSurface(
    MediaPipelineChainPlan& chain,
    const AVCodec& encoder,
    const AVCodecContext& openedHardwareContext,
    AVPixelFormat surfaceFormat)
{
    const AVPixFmtDescriptor* surfaceDescriptor =
        av_pix_fmt_desc_get(surfaceFormat);
    if (!surfaceDescriptor ||
        (surfaceDescriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0 ||
        !ffmpegCodecSupportsPixelFormat(&encoder, surfaceFormat)) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "opened hardware encoder exposes no advertised software-input packet-layout probe contract"));
    }
    if (!chain.encoder.encoderRateControl) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "hardware encoder packet-layout probe has no rate-control contract"));
    }

    auto probeContext = ::media::ffmpeg::makeCodecContext(&encoder);
    if (!probeContext) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "hardware encoder software-input packet-layout probe context"));
    }
    probeContext->width = openedHardwareContext.width;
    probeContext->height = openedHardwareContext.height;
    probeContext->pix_fmt = surfaceFormat;
    probeContext->sw_pix_fmt = surfaceFormat;
    probeContext->time_base = openedHardwareContext.time_base;
    probeContext->framerate = openedHardwareContext.framerate;
    probeContext->sample_aspect_ratio = openedHardwareContext.sample_aspect_ratio;
    probeContext->max_b_frames = openedHardwareContext.max_b_frames;

    auto applied = MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
        *probeContext, *chain.encoder.encoderRateControl);
    if (!applied) return ::media::Status::failure(applied.error());

    const int opened = avcodec_open2(probeContext.get(), &encoder, nullptr);
    if (opened < 0) {
        return FFmpegGraphError::statusFromCode(
            opened, "avcodec_open2(hardware encoder software-input packet-layout probe)");
    }
    return publishPacketLayout(chain, *probeContext);
}

MediaHardwareCapability unavailable(std::string reason)
{
    return {false, std::move(reason)};
}

MediaHardwareCapability ffmpegUnavailable(const std::string& operation, int code)
{
    return unavailable(operation + " failed: " + FFmpegGraphError::describe(code));
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

::media::ffmpeg::BufferRefPtr createRkmppProbeDevice(
    std::string& failure)
{
#if defined(__linux__)
    AVBufferRef* rawDevice = nullptr;
    const int result = av_hwdevice_ctx_create(
        &rawDevice, AV_HWDEVICE_TYPE_RKMPP, nullptr, nullptr, 0);
    ::media::ffmpeg::BufferRefPtr device(rawDevice);
    if (result < 0 || !device) {
        failure = FFmpegGraphError::describe(result);
        return {};
    }
    return device;
#else
    failure = "RKMPP device probing is unavailable on this platform";
    return {};
#endif
}

MediaHardwareCapability validateInternallyManagedRkmppChain(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    if (!chain.decoder.outputFrame || !chain.encoder.inputFrame) {
        return unavailable("RKMPP frame contracts are missing");
    }

    const AVPixelFormat decoderFormat =
        pixelFormat(chain.decoder.outputFrame->pixelFormat);
    const AVPixelFormat encoderFormat =
        pixelFormat(chain.encoder.inputFrame->pixelFormat);
    const AVPixelFormat encoderSurfaceFormat =
        pixelFormat(chain.encoder.inputFrame->surfacePixelFormat);
    if (decoderFormat != AV_PIX_FMT_DRM_PRIME ||
        encoderFormat != AV_PIX_FMT_DRM_PRIME ||
        encoderSurfaceFormat == AV_PIX_FMT_NONE) {
        return unavailable(
            "RKMPP codecs require advertised DRM PRIME frames and an explicit surface format");
    }

    const AVCodec* decoder =
        avcodec_find_decoder_by_name(chain.decoder.ffmpegName.c_str());
    if (!decoder) {
        return unavailable("planned decoder is unavailable: " + chain.decoder.ffmpegName);
    }
    if (!ffmpegCodecSupportsPixelFormat(decoder, decoderFormat)) {
        return unavailable("planned RKMPP decoder does not advertise DRM PRIME frames");
    }

    const AVCodec* encoder =
        avcodec_find_encoder_by_name(chain.encoder.ffmpegName.c_str());
    if (!encoder) {
        return unavailable("planned encoder is unavailable: " + chain.encoder.ffmpegName);
    }
    if (!ffmpegCodecSupportsPixelFormat(encoder, encoderFormat)) {
        return unavailable("planned RKMPP encoder does not advertise DRM PRIME frames");
    }

    if (chain.filterActive) {
        if (chain.filterImplementation != MediaVideoFilterImplementation::Rga ||
            chain.filter.filterName.empty() ||
            !chain.filter.inputFrame || !chain.filter.outputFrame) {
            return unavailable("planned RKMPP resize filter contract is incomplete");
        }
        auto probeFrame = ::media::ffmpeg::makeFrame();
        if (!probeFrame) {
            return unavailable("av_frame_alloc(RKMPP filter probe) returned null");
        }
        std::string rkmppDeviceFailure;
        auto rkmppDevice = createRkmppProbeDevice(rkmppDeviceFailure);
        if (!rkmppDevice) {
            return unavailable(
                "planned RKMPP RGA device probe failed: " +
                rkmppDeviceFailure);
        }
        std::string framesFailure;
        auto probeFrames = createFramesContext(
            rkmppDevice.get(), decoderFormat, encoderSurfaceFormat,
            chain.filter.inputFrame->size.width,
            chain.filter.inputFrame->size.height,
            4, framesFailure);
        if (!probeFrames) {
            return unavailable(
                "planned RKMPP RGA frames probe failed: " + framesFailure);
        }
        probeFrame->format = decoderFormat;
        probeFrame->width = chain.filter.inputFrame->size.width;
        probeFrame->height = chain.filter.inputFrame->size.height;
        probeFrame->sample_aspect_ratio = AVRational{1, 1};
        probeFrame->hw_frames_ctx = av_buffer_ref(probeFrames.get());
        if (!probeFrame->hw_frames_ctx) {
            return unavailable(
                "av_buffer_ref(RKMPP RGA probe frames context) returned null");
        }

        MediaNodeOptions filterOptions;
        filterOptions.set("filter.pipeline.filter", chain.filter.filterName);
        VideoFilterGraphBuildRequest request;
        request.options = &filterOptions;
        request.firstFrame = probeFrame.get();
        request.inputTimeBase =
            AVRational{options.sourceFrameRate.den, options.sourceFrameRate.num};
        request.inputFrameRate =
            AVRational{options.sourceFrameRate.num, options.sourceFrameRate.den};
        request.sampleAspectRatio = AVRational{1, 1};
        auto filterGraph = VideoFilterGraphBuilder::build(request);
        if (!filterGraph) {
            return unavailable(
                "planned RKMPP RGA graph negotiation failed: " +
                filterGraph.error().message);
        }
        const int sinkFormat = av_buffersink_get_format(
            filterGraph.value().bufferSink);
        const int sinkWidth = av_buffersink_get_w(
            filterGraph.value().bufferSink);
        const int sinkHeight = av_buffersink_get_h(
            filterGraph.value().bufferSink);
        if (sinkFormat != encoderFormat ||
            sinkWidth != chain.filter.outputFrame->size.width ||
            sinkHeight != chain.filter.outputFrame->size.height) {
            return unavailable(
                "planned RKMPP RGA graph negotiated a different output frame contract");
        }
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return unavailable("avcodec_alloc_context3(RKMPP decoder) returned null");
    }
    decoderContext->width = options.probeWidth;
    decoderContext->height = options.probeHeight;
    const int decoderOpened = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (decoderOpened < 0) {
        return ffmpegUnavailable(
            "avcodec_open2(decoder " + chain.decoder.ffmpegName + ")",
            decoderOpened);
    }

    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return unavailable("avcodec_alloc_context3(RKMPP encoder) returned null");
    }
    encoderContext->width =
        options.targetWidth > 0 ? options.targetWidth : options.probeWidth;
    encoderContext->height =
        options.targetHeight > 0 ? options.targetHeight : options.probeHeight;
    encoderContext->pix_fmt = encoderFormat;
    encoderContext->sw_pix_fmt = encoderSurfaceFormat;
    const MediaRational encoderFrameRate = options.targetFrameRate.isKnown()
        ? options.targetFrameRate : options.sourceFrameRate;
    encoderContext->time_base =
        AVRational{encoderFrameRate.den, encoderFrameRate.num};
    encoderContext->framerate =
        AVRational{encoderFrameRate.num, encoderFrameRate.den};
    encoderContext->sample_aspect_ratio = AVRational{1, 1};
    if (options.lowLatency) {
        encoderContext->max_b_frames = 0;
    }
    if (!chain.encoder.encoderRateControl) {
        return unavailable("RKMPP encoder emission contract is missing");
    }
    auto applied = MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
        *encoderContext, *chain.encoder.encoderRateControl);
    if (!applied) {
        return unavailable(applied.error().message);
    }
    const int encoderOpened = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (encoderOpened < 0) {
        return ffmpegUnavailable(
            "avcodec_open2(encoder " + chain.encoder.ffmpegName + ")",
            encoderOpened);
    }
    auto packetLayout = publishPacketLayout(chain, *encoderContext);
    if (!packetLayout) {
        packetLayout = publishPacketLayoutThroughAdvertisedSoftwareSurface(
            chain, *encoder, *encoderContext, encoderSurfaceFormat);
    }
    if (!packetLayout) return unavailable(packetLayout.error().message);

    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *encoderContext, *chain.encoder.encoderRateControl,
        encoderFrameRate, *chain.encoder.encodedPacketLayout,
        "opened-encoder-context:" + chain.encoder.ffmpegName,
        "rkmpp");
    if (!emission) {
        return unavailable(emission.error().message);
    }
    chain.encoder.preparedEmission = std::move(emission).value();
    return {true, chain.filterActive
                      ? "internally managed RKMPP codecs and planned RGA graph negotiated"
                      : "internally managed RKMPP codecs opened without a filter"};
}

MediaHardwareCapability validateSoftwareEncoder(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    if (!chain.encoder.inputFrame || !chain.encoder.encoderRateControl) {
        return unavailable("software encoder preflight contract is missing");
    }
    const AVCodec* encoder =
        avcodec_find_encoder_by_name(chain.encoder.ffmpegName.c_str());
    if (!encoder) {
        return unavailable("planned encoder is unavailable: " +
                           chain.encoder.ffmpegName);
    }
    auto context = ::media::ffmpeg::makeCodecContext(encoder);
    if (!context) {
        return unavailable("avcodec_alloc_context3(software encoder) returned null");
    }
    const auto cadence = options.targetFrameRate.isKnown()
        ? options.targetFrameRate : options.sourceFrameRate;
    context->width = options.targetWidth > 0
        ? options.targetWidth : options.probeWidth;
    context->height = options.targetHeight > 0
        ? options.targetHeight : options.probeHeight;
    context->pix_fmt = pixelFormat(chain.encoder.inputFrame->pixelFormat);
    context->time_base = AVRational{cadence.den, cadence.num};
    context->framerate = AVRational{cadence.num, cadence.den};
    context->sample_aspect_ratio = AVRational{1, 1};
    if (context->width <= 0 || context->height <= 0 ||
        context->pix_fmt == AV_PIX_FMT_NONE || !cadence.isKnown()) {
        return unavailable("software encoder preflight geometry is incomplete");
    }
    auto applied = MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
        *context, *chain.encoder.encoderRateControl);
    if (!applied) return unavailable(applied.error().message);
    const int opened = avcodec_open2(context.get(), encoder, nullptr);
    if (opened < 0) {
        return ffmpegUnavailable(
            "avcodec_open2(software encoder " + chain.encoder.ffmpegName + ")",
            opened);
    }
    auto packetLayout = publishPacketLayout(chain, *context);
    if (!packetLayout) return unavailable(packetLayout.error().message);
    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *context, *chain.encoder.encoderRateControl, cadence,
        *chain.encoder.encodedPacketLayout,
        "opened-encoder-context:" + chain.encoder.ffmpegName, "ffmpeg-software");
    if (!emission) return unavailable(emission.error().message);
    chain.encoder.preparedEmission = std::move(emission).value();
    return {true, "software encoder opened with effective emission readback"};
}

MediaHardwareCapability validateCompleteChain(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    if (!chain.allHardware) {
        return validateSoftwareEncoder(chain, options);
    }
    if (!chain.sameHardwareDevice) {
        return unavailable(
            "hardware chain validation requires one hardware device across all active stages");
    }
    if (options.probeWidth <= 0 || options.probeHeight <= 0) {
        return unavailable("hardware chain validation requires planner-resolved probe dimensions");
    }
    if (!options.sourceFrameRate.isKnown()) {
        return unavailable("hardware chain validation requires planner-resolved source frame rate");
    }

    if (chain.decoder.deviceKind() == MediaHardwareDeviceKind::RKMPP) {
        return validateInternallyManagedRkmppChain(chain, options);
    }

    if (!chain.decoder.outputFrame || !chain.encoder.inputFrame) {
        return unavailable("hardware chain requires decoder output and encoder input frame contracts");
    }

    const int outputWidth = options.targetWidth > 0 ? options.targetWidth : options.probeWidth;
    const int outputHeight = options.targetHeight > 0 ? options.targetHeight : options.probeHeight;
    const AVPixelFormat hardwareFormat = pixelFormat(chain.encoder.inputFrame->pixelFormat);
    const AVPixelFormat surfaceFormat = pixelFormat(chain.encoder.inputFrame->surfacePixelFormat);
    const AVPixelFormat encoderFormat = pixelFormat(chain.encoder.inputFrame->pixelFormat);
    if (encoderFormat == AV_PIX_FMT_NONE) {
        return unavailable("planned encoder pixel format is not recognized by FFmpeg");
    }

    AVHWDeviceType deviceType = AV_HWDEVICE_TYPE_NONE;
    ::media::ffmpeg::BufferRefPtr device;
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
    encoderContext->sw_pix_fmt = surfaceFormat;
    const MediaRational encoderFrameRate = options.targetFrameRate.isKnown()
        ? options.targetFrameRate : options.sourceFrameRate;
    encoderContext->time_base =
        AVRational{encoderFrameRate.den, encoderFrameRate.num};
    encoderContext->framerate =
        AVRational{encoderFrameRate.num, encoderFrameRate.den};
    encoderContext->sample_aspect_ratio = AVRational{1, 1};
    if (options.lowLatency) {
        encoderContext->max_b_frames = 0;
    }
    if (!chain.encoder.encoderRateControl) {
        return unavailable("hardware encoder emission contract is missing");
    }
    auto applied = MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
        *encoderContext, *chain.encoder.encoderRateControl);
    if (!applied) return unavailable(applied.error().message);

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

    if (chain.filterActive) {
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
            AVRational{options.sourceFrameRate.den, options.sourceFrameRate.num};
        request.inputFrameRate =
            AVRational{options.sourceFrameRate.num, options.sourceFrameRate.den};
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
    auto packetLayout = publishPacketLayout(chain, *encoderContext);
    if (!packetLayout) return unavailable(packetLayout.error().message);

    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *encoderContext, *chain.encoder.encoderRateControl,
        encoderFrameRate, *chain.encoder.encodedPacketLayout,
        "opened-encoder-context:" + chain.encoder.ffmpegName,
        chain.decoder.hwaccelName);
    if (!emission) return unavailable(emission.error().message);
    chain.encoder.preparedEmission = std::move(emission).value();

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
    out << "backend=" << mediaHardwareDeviceKindName(chain.decoder.deviceKind())
        << " status=" << (capability.available ? "found" : "not_found")
        << " probe=decoder_filter_encoder_open"
        << " note=" << capability.reason;
    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::PlannerCapability,
                            out.str());

    chain.decoder.available = capability.available;
    chain.decoder.availabilityReason = capability.reason;
    if (chain.filterActive) {
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
    if (!chain.encoder.preparedEmission ||
        chain.encoder.preparedEmission->authority.empty() ||
        chain.encoder.preparedEmission->backend.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "encoder preflight succeeded without effective emission readback"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
