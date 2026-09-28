#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/planner/capability/MediaVideoEncoderCapabilityProbe.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"

#include "internal/graph/builder/video/VideoFilterGraphBuilder.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
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

enum class MediaCapabilityProbeScope { CompletePipeline, OutputBranch };

::media::Result<MediaVideoSourceFilterNegotiation> negotiateSourceFilterDescription(
    const MediaVideoSourcePlan& source, const AVFrame& firstFrame,
    const MediaRational& inputTimeBase, const MediaRational& inputFrameRate,
    const MediaRational& sampleAspectRatio, const MediaHardwareDescriptor& target);

::media::Result<MediaVideoSourceFilterNegotiation> negotiateLegacyChainFilter(
    const MediaPipelineChainPlan& chain, const AVFrame& frame, const MediaRational& cadence)
{
    using Result = ::media::Result<MediaVideoSourceFilterNegotiation>;
    if (!chain.filter.outputFrame || !chain.encoder.inputFrame)
        return Result::failure(::media::ErrorInfo::invalidArgument("Chain filter output contract is missing"));
    // Existing complete-chain probes supply synthetic descriptions, not decoded frames.
    auto result = negotiateSourceFilterDescription(chain, frame,
        {cadence.den, cadence.num}, cadence, {1, 1}, *chain.filter.outputFrame);
    if (!result) return result;
    const auto& target = *chain.encoder.inputFrame;
    if (result.value().pixelFormat != target.pixelFormat ||
        result.value().outputSize != target.size)
        return Result::failure(::media::ErrorInfo::hardwareUnavailable(
            "Source filter negotiation differs from encoder input"));
    return result;
}

MediaHardwareCapability unavailable(std::string reason)
{
    return {false, std::move(reason)};
}

MediaRational capabilityInputFrameRate(
    const MediaPipelinePlannerOptions& options) noexcept
{
    return options.sourceFrameRate.isKnown()
        ? options.sourceFrameRate
        : options.targetFrameRate;
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

::media::Result<std::optional<MediaDecoderInputRetention>> probeDecoderOpenRetention(
    const AVCodec& decoder, const MediaPipelineStagePlan& stage,
    MediaSize size, AVBufferRef* device)
{
    using Result = ::media::Result<std::optional<MediaDecoderInputRetention>>;
    auto context = ::media::ffmpeg::makeCodecContext(&decoder);
    if (!context) return Result::failure(::media::ErrorInfo::internalError(
        "Decoder retention probe allocation failed"));
    context->width = size.width;
    context->height = size.height;
    if (device) {
        context->hw_device_ctx = av_buffer_ref(device);
        if (!context->hw_device_ctx) return Result::failure(::media::ErrorInfo::internalError(
            "Decoder retention probe device reference allocation failed"));
    }
    const int opened = avcodec_open2(context.get(), &decoder, nullptr);
    if (opened < 0) return Result::failure(FFmpegGraphError::fromCode(opened, "Decoder retention probe open"));
    return Result::success(MediaDecoderInputRetentionAdapter::readAfterOpen(*context, stage.hwaccelName));
}

MediaHardwareCapability validateInternallyManagedRkmppChain(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options,
    AVBufferRef* runningFrames,
    MediaCapabilityProbeScope scope)
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
        auto rkmppDevice = runningFrames
            ? ::media::ffmpeg::BufferRefPtr(av_buffer_ref(
                  reinterpret_cast<AVHWFramesContext*>(runningFrames->data)->device_ref))
            : createRkmppProbeDevice(rkmppDeviceFailure);
        if (!rkmppDevice) {
            return unavailable(
                "planned RKMPP RGA device probe failed: " +
                rkmppDeviceFailure);
        }
        std::string framesFailure;
        auto probeFrames = runningFrames
            ? ::media::ffmpeg::BufferRefPtr(av_buffer_ref(runningFrames))
            : createFramesContext(
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

        const auto inputFrameRate = capabilityInputFrameRate(options);
        auto negotiation = negotiateLegacyChainFilter(chain, *probeFrame, inputFrameRate);
        if (!negotiation) return unavailable(negotiation.error().message);
    }

    if (scope == MediaCapabilityProbeScope::CompletePipeline) {
        auto retention = probeDecoderOpenRetention(*decoder, chain.decoder,
            {options.probeWidth, options.probeHeight}, nullptr);
        if (!retention) return unavailable(retention.error().message);
        chain.decoder.preparedInputRetention = std::move(retention).value();
    }

    auto evidence = MediaVideoEncoderCapabilityProbe::inspect(chain.encoder,
        {{1, 1}, options.sourceColorRange, nullptr, nullptr, "rkmpp", encoderSurfaceFormat});
    if (!evidence) return unavailable(evidence.error().message);
    return {true, chain.filterActive
                      ? "internally managed RKMPP codecs and planned RGA graph negotiated"
                      : "internally managed RKMPP codecs opened without a filter"};
}

MediaHardwareCapability validateSoftwareEncoder(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    auto evidence = MediaVideoEncoderCapabilityProbe::inspect(chain.encoder,
        {{1, 1}, options.sourceColorRange, nullptr, nullptr, "ffmpeg-software", std::nullopt});
    if (!evidence) return unavailable(evidence.error().message);
    return {true, "software encoder opened with effective emission readback"};
}

MediaHardwareCapability validateCompleteChain(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options,
    AVBufferRef* runningFrames,
    MediaCapabilityProbeScope scope)
{
    if (!chain.allHardware) {
        if (scope == MediaCapabilityProbeScope::CompletePipeline) {
            const auto* decoder = avcodec_find_decoder_by_name(chain.decoder.ffmpegName.c_str());
            if (!decoder) return unavailable("planned decoder is unavailable");
            auto retention = probeDecoderOpenRetention(*decoder, chain.decoder,
                {options.probeWidth, options.probeHeight}, nullptr);
            if (!retention) return unavailable(retention.error().message);
            chain.decoder.preparedInputRetention = std::move(retention).value();
        }
        return validateSoftwareEncoder(chain, options);
    }
    if (!chain.sameHardwareDevice) {
        return unavailable(
            "hardware chain validation requires one hardware device across all active stages");
    }
    if (options.probeWidth <= 0 || options.probeHeight <= 0) {
        return unavailable("hardware chain validation requires planner-resolved probe dimensions");
    }
    if (!capabilityInputFrameRate(options).isKnown()) {
        return unavailable(
            "hardware chain validation requires planner-resolved source or target frame rate");
    }

    if (chain.decoder.deviceKind() == MediaHardwareDeviceKind::RKMPP) {
        return validateInternallyManagedRkmppChain(chain, options, runningFrames, scope);
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

    if (runningFrames) {
        device.reset(av_buffer_ref(
            reinterpret_cast<AVHWFramesContext*>(runningFrames->data)->device_ref));
        if (!device) return unavailable("running decoder device reference allocation failed");
    } else {
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

    if (scope == MediaCapabilityProbeScope::CompletePipeline) {
        auto retention = probeDecoderOpenRetention(*decoder, chain.decoder,
            {options.probeWidth, options.probeHeight}, device.get());
        if (!retention) return unavailable(retention.error().message);
        chain.decoder.preparedInputRetention = std::move(retention).value();
    }

    ::media::ffmpeg::BufferRefPtr sourceFrames;
    if (device) {
        if (hardwareFormat == AV_PIX_FMT_NONE || surfaceFormat == AV_PIX_FMT_NONE) {
            return unavailable(
                "hardware chain requires planned hardware and surface pixel formats");
        }
        std::string framesFailure;
        sourceFrames = runningFrames
            ? ::media::ffmpeg::BufferRefPtr(av_buffer_ref(runningFrames))
            : createFramesContext(
            device.get(), hardwareFormat, surfaceFormat,
            options.probeWidth, options.probeHeight, 4, framesFailure);
        if (!sourceFrames) {
            return unavailable("decoder/filter frame negotiation " + framesFailure);
        }
    }

    ::media::ffmpeg::BufferRefPtr encoderFrames;
    if (device) {
        std::string framesFailure;
        encoderFrames = createFramesContext(
            device.get(), hardwareFormat, surfaceFormat,
            outputWidth, outputHeight, 4, framesFailure);
        if (!encoderFrames) {
            return unavailable("filter/encoder frame negotiation " + framesFailure);
        }
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

        const auto inputFrameRate = capabilityInputFrameRate(options);
        auto negotiation = negotiateLegacyChainFilter(chain, *firstFrame, inputFrameRate);
        if (!negotiation) return unavailable(negotiation.error().message);
    }

    auto evidence = MediaVideoEncoderCapabilityProbe::inspect(chain.encoder,
        {{1, 1}, options.sourceColorRange, device.get(), encoderFrames.get(),
         chain.encoder.hwaccelName, std::nullopt});
    if (!evidence) return unavailable(evidence.error().message);

    return {true, "decoder/filter/encoder chain opened and negotiated"};
}

::media::Result<MediaVideoSourceFilterNegotiation>
negotiateSourceFilterDescription(
    const MediaVideoSourcePlan& source, const AVFrame& firstFrame,
    const MediaRational& inputTimeBase, const MediaRational& inputFrameRate,
    const MediaRational& sampleAspectRatio, const MediaHardwareDescriptor& target)
{
    using Result = ::media::Result<MediaVideoSourceFilterNegotiation>;
    if (!source.filterActive || source.filter.filterName.empty() ||
        !source.filter.inputFrame || !source.filter.outputFrame ||
        inputTimeBase.num <= 0 || inputTimeBase.den <= 0 ||
        inputFrameRate.num <= 0 || inputFrameRate.den <= 0 ||
        sampleAspectRatio.num <= 0 || sampleAspectRatio.den <= 0 ||
        firstFrame.width != source.filter.inputFrame->size.width ||
        firstFrame.height != source.filter.inputFrame->size.height ||
        firstFrame.format != pixelFormat(source.filter.inputFrame->pixelFormat) ||
        (source.filter.inputFrame->requiresHardwareFramesContext && !firstFrame.hw_frames_ctx) ||
        target.size.width <= 0 || target.size.height <= 0 || pixelFormat(target.pixelFormat) == AV_PIX_FMT_NONE ||
        *source.filter.outputFrame != target)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source filter negotiation requires explicit frame, timing, SAR and target contracts"));
    MediaNodeOptions filterOptions;
    filterOptions.set("filter.pipeline.filter", source.filter.filterName);
    VideoFilterGraphBuildRequest request;
    request.options = &filterOptions;
    request.firstFrame = &firstFrame;
    request.inputTimeBase = {inputTimeBase.num, inputTimeBase.den};
    request.inputFrameRate = {inputFrameRate.num, inputFrameRate.den};
    request.sampleAspectRatio = {sampleAspectRatio.num, sampleAspectRatio.den};
    auto graph = VideoFilterGraphBuilder::build(request);
    if (!graph) return Result::failure(graph.error());
    const auto format = static_cast<AVPixelFormat>(av_buffersink_get_format(graph.value().bufferSink));
    const MediaSize size{av_buffersink_get_w(graph.value().bufferSink),
                         av_buffersink_get_h(graph.value().bufferSink)};
    if (format != pixelFormat(target.pixelFormat) || size.width != target.size.width ||
        size.height != target.size.height)
        return Result::failure(::media::ErrorInfo::hardwareUnavailable(
            "Source filter graph negotiated a different target format or size"));
    const auto sar = av_buffersink_get_sample_aspect_ratio(graph.value().bufferSink);
    return Result::success({size, target.pixelFormat, {sar.num, sar.den}});
}

} // namespace

::media::Result<MediaVideoSourceFilterNegotiation>
MediaHardwareCapabilityProbe::negotiateSourceFilter(
    const MediaVideoSourcePlan& source, const AVFrame& firstFrame,
    const MediaRational& inputTimeBase, const MediaRational& inputFrameRate,
    const MediaRational& sampleAspectRatio, const MediaHardwareDescriptor& target)
{
    using Result = ::media::Result<MediaVideoSourceFilterNegotiation>;
    if (!source.filter.inputFrame)
        return Result::failure(::media::ErrorInfo::invalidArgument("Source filter input contract is missing"));
    auto frame = MediaVideoFrameContractValidator::validate(
        firstFrame, *source.filter.inputFrame, "source filter negotiation");
    if (!frame) return Result::failure(frame.error());
    if (sampleAspectRatio.num <= 0 || sampleAspectRatio.den <= 0 ||
        firstFrame.sample_aspect_ratio.num <= 0 || firstFrame.sample_aspect_ratio.den <= 0 ||
        av_cmp_q(firstFrame.sample_aspect_ratio, {sampleAspectRatio.num, sampleAspectRatio.den}) != 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source filter negotiation SAR differs from the supplied actual frame"));
    return negotiateSourceFilterDescription(source, firstFrame, inputTimeBase,
        inputFrameRate, sampleAspectRatio, target);
}

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
    : m_chainValidator([](MediaPipelineChainPlan& chain,
                         const MediaPipelinePlannerOptions& options) {
          return validateCompleteChain(chain, options, nullptr,
              MediaCapabilityProbeScope::CompletePipeline);
      })
{
}

MediaHardwareCapability MediaHardwareCapabilityProbe::validateOutputBranch(
    MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options,
    AVBufferRef* runningFrames,
    const MediaDecoderRuntimeFacts& decoderFacts,
    const MediaVideoSharedSourcePlan& sourceAllocation)
{
    if (!chain.decoder.outputFrame) return unavailable("output branch lacks running decoder frame facts");
    if (decoderFacts.decoderName.empty() || decoderFacts.decoderName != chain.decoder.ffmpegName)
        return unavailable("opened decoder identity differs from the shared source contract");
    const bool independentFilterOutput =
        sourceAllocation.allocation == MediaVideoSourceAllocation::IndependentFilterOutput;
    if (independentFilterOutput != sourceAllocation.copy.has_value() ||
        (independentFilterOutput && (!sourceAllocation.copy->valid() ||
            sourceAllocation.copy->timing != MediaVideoFilterTimingAuthority::SourceFrame)))
        return unavailable("shared source allocation differs from its filter execution evidence");
    const auto& expected = *chain.decoder.outputFrame;
    if (expected.isHardwareBacked()) {
        // cuvid_output_frame always copies into independently allocated CUDA
        // storage. NVDEC's nvdec_retrieve_data makes the frame writable before
        // publishing it unless the opened decoder explicitly requests UNSAFE_OUTPUT.
        // Both paths release the mapped fixed decoder surface before downstream use.
        const bool independentCudaOutput = sourceAllocation.allocation == MediaVideoSourceAllocation::DecoderOutput &&
            expected.deviceKind == MediaHardwareDeviceKind::CUDA &&
            (decoderFacts.decoderName.ends_with("_cuvid") ||
             (decoderFacts.hardwareAccelerationFlags & AV_HWACCEL_FLAG_UNSAFE_OUTPUT) == 0);
        if (!independentCudaOutput && !independentFilterOutput) {
            return unavailable(
                "dynamic hardware output lacks authoritative independent decoder-output allocation or fixed-pool headroom evidence");
        }
        auto frames = MediaVideoFrameContractValidator::validateHardwareFrames(
            runningFrames, expected, "output branch running decoder");
        if (!frames) return unavailable(frames.error().describe());
    } else if (runningFrames) {
        return unavailable("software decoder branch rejects hardware frames evidence");
    }
    return validateCompleteChain(chain, options, runningFrames,
        MediaCapabilityProbeScope::OutputBranch);
}

MediaHardwareCapabilityProbe::MediaHardwareCapabilityProbe(
    ChainValidator chainValidator)
    : m_chainValidator(std::move(chainValidator)),
      m_suppliedValidator(static_cast<bool>(m_chainValidator))
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
