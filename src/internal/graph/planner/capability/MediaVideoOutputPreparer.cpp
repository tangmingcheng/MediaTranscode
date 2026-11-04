#include "internal/graph/planner/capability/MediaVideoOutputPreparer.h"
#include "internal/graph/planner/capability/MediaVideoEncoderCapabilityProbe.h"
#include "internal/graph/planner/video/MediaVideoOutputPlanner.h"
#include "internal/graph/planner/video/MediaVideoColorContractPlanner.h"
#include "internal/graph/builder/codec/MediaVideoEncoderPlanOptionCodec.h"
#include "internal/graph/runtime/ffmpeg/MediaHardwareFramesAllocation.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<::media::ffmpeg::BufferRefPtr> MediaVideoOutputPreparer::openDevice(
    const MediaPipelineStagePlan& encoder, const MediaPreparationControl& control)
{
    using Result = ::media::Result<::media::ffmpeg::BufferRefPtr>;
    if (auto status = control.check("composition hardware device preparation"); !status)
        return Result::failure(status.error());
    const auto type = av_hwdevice_find_type_by_name(encoder.hwaccelName.c_str());
    if (encoder.role != MediaPipelineStageRole::Encoder || type == AV_HWDEVICE_TYPE_NONE)
        return Result::failure(::media::ErrorInfo::unsupported("Composition candidate has no hardware device type"));
    AVBufferRef* raw = nullptr;
    const int created = av_hwdevice_ctx_create(&raw, type, nullptr, nullptr, 0);
    ::media::ffmpeg::BufferRefPtr device(raw);
    if (created < 0) return Result::failure(FFmpegGraphError::fromCode(created, "output hardware device"));
    if (!device) return Result::failure(::media::ErrorInfo::allocationFailed("output hardware device"));
    return Result::success(std::move(device));
}

::media::Result<MediaProbedVideoOutput> MediaVideoOutputPreparer::probe(
    MediaPipelineStagePlan encoder, MediaEncoderOpenContract open,
    MediaVideoEncoderFrameInput frameInput, ::media::ffmpeg::BufferRefPtr device,
    const MediaPreparationControl& control)
{
    using Result = ::media::Result<MediaProbedVideoOutput>;
    if (auto status = control.check("output encoder candidate preparation"); !status)
        return Result::failure(status.error());
    if (encoder.role != MediaPipelineStageRole::Encoder || !encoder.inputFrame ||
        encoder.ffmpegName != open.codecName || encoder.hwaccelName.empty() ||
        frameInput.sampleAspectRatio.num <= 0 || frameInput.sampleAspectRatio.den <= 0 ||
        (frameInput.colorRange != AVCOL_RANGE_MPEG && frameInput.colorRange != AVCOL_RANGE_JPEG))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output preparation requires selected hardware encoder and authoritative frame properties"));
    const auto& frame = *encoder.inputFrame;
    const auto hardwareFormat = av_get_pix_fmt(frame.pixelFormat.c_str());
    const auto softwareFormat = av_get_pix_fmt(frame.surfacePixelFormat.c_str());
    const auto deviceType = av_hwdevice_find_type_by_name(encoder.hwaccelName.c_str());
    if (deviceType == AV_HWDEVICE_TYPE_NONE || hardwareFormat == AV_PIX_FMT_NONE ||
        softwareFormat == AV_PIX_FMT_NONE || frame.size.width != open.width || frame.size.height != open.height)
        return Result::failure(::media::ErrorInfo::unsupported("Output candidate lacks hardware pool geometry"));
    if (!device || !device->data ||
        reinterpret_cast<const AVHWDeviceContext*>(device->data)->type != deviceType)
        return Result::failure(::media::ErrorInfo::invalidArgument("Output probe requires the actual source preparation device"));
    const MediaVideoColorContract color{frameInput.colorRange, frameInput.colorPrimaries,
        frameInput.colorTransfer, frameInput.colorSpace};
    if (auto status = MediaVideoColorContractPlanner::validate(color); !status)
        return Result::failure(status.error());
    // The existing packet-layout probe sends one frame then drains; this pool
    // supplies precisely that one submission and never becomes the production pool.
    constexpr int PacketLayoutProbeSubmissions = 1;
    auto frames = allocateHardwareFramesContext(device.get(), hardwareFormat, softwareFormat,
        open.width, open.height, PacketLayoutProbeSubmissions);
    if (!frames) return Result::failure(frames.error());
    auto sample = ::media::ffmpeg::makeFrame();
    if (!sample) return Result::failure(::media::ErrorInfo::allocationFailed("Output probe surface readback"));
    const int allocated = av_hwframe_get_buffer(frames.value().get(), sample.get(), 0);
    if (allocated < 0) return Result::failure(FFmpegGraphError::fromCode(allocated, "output probe surface readback"));
    auto allocation = readMediaVideoCanvasAllocation(*sample);
    if (!allocation) return Result::failure(allocation.error());
    sample.reset();
    encoder.encoderRateControl = open.rateControl;
    encoder.encoderOpenContract = std::move(open);
    const auto* codec = avcodec_find_encoder_by_name(encoder.ffmpegName.c_str());
    std::optional<AVPixelFormat> equivalent;
    if (ffmpegCodecSupportsPixelFormat(codec, softwareFormat)) equivalent = softwareFormat;
    if (auto status = MediaVideoEncoderCapabilityProbe::inspect(encoder,
            {frameInput.sampleAspectRatio, frameInput.colorRange, device.get(), frames.value().get(),
             encoder.hwaccelName, equivalent, color}); !status) return Result::failure(status.error());
    if (auto status = control.check("output encoder candidate readback"); !status)
        return Result::failure(status.error());
    if (!encoder.effectiveColorRange || !encoder.effectiveColorRange->color ||
        *encoder.effectiveColorRange->color != color)
        return Result::failure(::media::ErrorInfo::unsupported(
            "Output encoded SPS does not preserve the prepared composition color contract"));
    // The successful actual open above is evidence for an externally supplied pool.
    encoder.inputFrame->requiresHardwareDeviceContext = true;
    encoder.inputFrame->requiresHardwareFramesContext = true;
    encoder.available = true;
    MediaVideoOutputPlan output{std::move(encoder)};
    if (auto status = MediaVideoOutputPlanner::materializeExecutionContract(output); !status)
        return Result::failure(status.error());
    if (auto status = MediaVideoOutputPlanner::completePreparedRateControl(output); !status)
        return Result::failure(status.error());
    return Result::success({std::move(output), std::move(device), frameInput, allocation.value()});
}

::media::Result<MediaPreparedVideoEncoder> MediaVideoOutputPreparer::prepare(
    const MediaProbedVideoOutput& probed, const MediaVideoOutputPreparationPlan& plan,
    const std::shared_ptr<MediaPreparationStorageBudget>& storage,
    const MediaPreparationControl& control)
{
    using Result = ::media::Result<MediaPreparedVideoEncoder>;
    if (auto status = control.check("production output encoder preparation"); !status)
        return Result::failure(status.error());
    const auto& pool = plan.pool();
    if (!probed.device || !storage || pool.initialPoolSurfaces == 0 || pool.authority.empty() ||
        pool.initialPoolSurfaces > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))
        return Result::failure(::media::ErrorInfo::invalidArgument("Production output requires its admitted pool contract"));
    if (probed.allocation.surfaceBytes != plan.allocation().surfaceBytes ||
        probed.allocation.stagingBytes != plan.allocation().stagingBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument("Output allocation witness differs from frozen topology preparation"));
    auto lease = storage->reserve(plan.storageBytes(), MediaStorageAdmission::RequiredAllocation);
    if (!lease) return Result::failure(lease.error());
    auto options = MediaVideoEncoderPlanOptionCodec::encode(probed.plan.encoder);
    if (!options) return Result::failure(options.error());
    options.value().set("encoder.hardware_frames.initial_pool_surfaces", std::to_string(pool.initialPoolSurfaces));
    options.value().set("encoder.hardware_frames.pool_authority", pool.authority);
    auto prepared = MediaVideoEncoderPreparer::prepare(
        {probed.frameInput, &options.value(), probed.device.get(), probed.plan.encoderLineagePropagation}, probed.plan.encoder);
    if (!prepared) return Result::failure(prepared.error());
    const auto& context = *prepared.value().context;
    const MediaVideoColorContract actual{context.color_range, context.color_primaries,
        context.color_trc, context.colorspace};
    const auto& expected = probed.plan.encoder.effectiveColorRange;
    if (!expected || !expected->color || actual != *expected->color ||
        (prepared.value().readback.effectiveColorRange &&
         prepared.value().readback.effectiveColorRange->color != expected->color))
        return Result::failure(::media::ErrorInfo::unsupported(
            "Production encoder readback differs from the composition color contract"));
    if (auto status = control.check("production output encoder readback"); !status)
        return Result::failure(status.error());
    prepared.value().context.get_deleter().storageOwner =
        std::make_shared<MediaPreparationStorageLease>(std::move(lease).value());
    return prepared;
}
} // namespace media::ffmpeg::graph
