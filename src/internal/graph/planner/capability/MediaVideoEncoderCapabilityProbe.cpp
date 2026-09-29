#include "internal/graph/planner/capability/MediaVideoEncoderCapabilityProbe.h"

#include "internal/graph/planner/capability/MediaEncoderOpenContractAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"
#include "internal/graph/planner/capability/MediaEncoderRandomAccessAdapter.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status publishPacketLayout(
    MediaPipelineStagePlan& encoder, AVCodecContext& context)
{
    auto layout = MediaEncoderPacketLayoutCapabilityProvider::probeOpenedContext(
        context, encoder.effectiveColorRange);
    if (!layout) return ::media::Status::failure(layout.error());
    auto randomAccess = MediaEncoderRandomAccessAdapter::readAfterOpen(context);
    if (!randomAccess) return ::media::Status::failure(randomAccess.error());
    encoder.randomAccess = std::move(randomAccess).value();
    encoder.encodedPacketLayout = std::move(layout).value();
    return ::media::Status::success();
}

::media::Status probeEquivalentSoftwareSurface(
    MediaPipelineStagePlan& encoder, const AVCodecContext& hardwareContext,
    AVPixelFormat surfaceFormat)
{
    const auto* descriptor = av_pix_fmt_desc_get(surfaceFormat);
    if (!hardwareContext.codec || !descriptor ||
        (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0 ||
        !ffmpegCodecSupportsPixelFormat(hardwareContext.codec, surfaceFormat)) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "opened hardware encoder exposes no advertised software-input packet-layout probe contract"));
    }
    auto probeContext = ::media::ffmpeg::makeCodecContext(hardwareContext.codec);
    if (!probeContext) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "hardware encoder software-input packet-layout probe context"));
    }
    probeContext->pix_fmt = surfaceFormat;
    probeContext->sw_pix_fmt = surfaceFormat;
    probeContext->sample_aspect_ratio = hardwareContext.sample_aspect_ratio;
    probeContext->color_range = hardwareContext.color_range;
    auto applied = MediaEncoderOpenContractAdapter::applyBeforeOpen(
        *probeContext, *encoder.encoderOpenContract);
    if (!applied) return applied;
    const int opened = avcodec_open2(probeContext.get(), hardwareContext.codec, nullptr);
    if (opened < 0) {
        return FFmpegGraphError::statusFromCode(
            opened, "avcodec_open2(hardware encoder software-input packet-layout probe)");
    }
    if (auto equivalent = MediaEncoderOpenContractAdapter::validateEquivalentReadback(
            hardwareContext, *probeContext, *encoder.encoderOpenContract);
        !equivalent) return equivalent;
    return publishPacketLayout(encoder, *probeContext);
}

} // namespace

::media::Status MediaVideoEncoderCapabilityProbe::inspect(
    MediaPipelineStagePlan& encoder, const MediaVideoEncoderProbeInput& input)
{
    if (encoder.role != MediaPipelineStageRole::Encoder || !encoder.inputFrame ||
        !encoder.encoderOpenContract || !encoder.encoderRateControl ||
        input.sampleAspectRatio.num < 0 || input.sampleAspectRatio.den <= 0 || input.backend.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "encoder capability probe requires explicit frame and open contracts"));
    }
    const auto& frame = *encoder.inputFrame;
    const auto& open = *encoder.encoderOpenContract;
    const auto format = av_get_pix_fmt(frame.pixelFormat.c_str());
    const auto surface = frame.surfacePixelFormat.empty()
        ? AV_PIX_FMT_NONE : av_get_pix_fmt(frame.surfacePixelFormat.c_str());
    if (format == AV_PIX_FMT_NONE || frame.size.width != open.width ||
        frame.size.height != open.height ||
        (!frame.surfacePixelFormat.empty() && surface == AV_PIX_FMT_NONE) ||
        (frame.requiresHardwareDeviceContext && !input.hardwareDevice) ||
        (frame.requiresHardwareFramesContext && !input.hardwareFrames)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "encoder capability probe frame resources contradict the selected contract"));
    }
    if (input.hardwareFrames) {
        const auto* frames = reinterpret_cast<const AVHWFramesContext*>(input.hardwareFrames->data);
        if (!frames || !frames->device_ref || !input.hardwareDevice ||
            frames->device_ref->data != input.hardwareDevice->data ||
            frames->format != format || frames->sw_format != surface ||
            frames->width < open.width || frames->height < open.height) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "encoder capability probe frame pool differs from its device or geometry"));
        }
    }
    const auto* codec = avcodec_find_encoder_by_name(encoder.ffmpegName.c_str());
    if (!codec) return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable(
        "planned encoder is unavailable: " + encoder.ffmpegName));
    auto context = ::media::ffmpeg::makeCodecContext(codec);
    if (!context) return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
        "encoder capability probe context"));
    context->pix_fmt = format;
    context->sw_pix_fmt = surface;
    context->sample_aspect_ratio = {
        input.sampleAspectRatio.num, input.sampleAspectRatio.den};
    if (input.colorRange) context->color_range = *input.colorRange;
    if (auto applied = MediaEncoderOpenContractAdapter::applyBeforeOpen(*context, open);
        !applied) return applied;
    if (input.hardwareDevice) {
        context->hw_device_ctx = av_buffer_ref(input.hardwareDevice);
        if (!context->hw_device_ctx) return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("encoder probe device reference"));
    }
    if (input.hardwareFrames) {
        context->hw_frames_ctx = av_buffer_ref(input.hardwareFrames);
        if (!context->hw_frames_ctx) return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("encoder probe frames reference"));
    }
    const int opened = avcodec_open2(context.get(), codec, nullptr);
    if (opened < 0) return FFmpegGraphError::statusFromCode(
        opened, "avcodec_open2(encoder " + encoder.ffmpegName + ")");
    auto layout = publishPacketLayout(encoder, *context);
    if (!layout && input.advertisedEquivalentSoftwareSurface) {
        layout = probeEquivalentSoftwareSurface(
            encoder, *context, *input.advertisedEquivalentSoftwareSurface);
    }
    if (!layout) return layout;
    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *context, *encoder.encoderRateControl, open.frameRate, *encoder.encodedPacketLayout,
        "opened-encoder-context:" + encoder.ffmpegName, input.backend);
    if (!emission) return ::media::Status::failure(emission.error());
    encoder.preparedEmission = std::move(emission).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
