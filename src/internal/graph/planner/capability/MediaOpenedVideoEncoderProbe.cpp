#include "internal/graph/planner/capability/MediaOpenedVideoEncoderProbe.h"

#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderOpenContractAdapter.h"
#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"
#include "internal/graph/planner/capability/MediaEncoderRandomAccessAdapter.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecPixelFormatCapability.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
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

::media::Status MediaOpenedVideoEncoderProbe::inspect(
    MediaPipelineStagePlan& encoder, AVCodecContext& context,
    MediaRational frameRate, const std::string& backend,
    std::optional<AVPixelFormat> advertisedEquivalentSoftwareSurface)
{
    if (encoder.role != MediaPipelineStageRole::Encoder ||
        !context.codec || !av_codec_is_encoder(context.codec) ||
        encoder.ffmpegName != context.codec->name ||
        !encoder.encoderRateControl || !encoder.encoderOpenContract) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "opened encoder probe requires its selected codec, rate-control and open contracts"));
    }
    auto layout = publishPacketLayout(encoder, context);
    if (!layout && advertisedEquivalentSoftwareSurface) {
        layout = probeEquivalentSoftwareSurface(
            encoder, context, *advertisedEquivalentSoftwareSurface);
    }
    if (!layout) return layout;
    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        context, *encoder.encoderRateControl, frameRate, *encoder.encodedPacketLayout,
        "opened-encoder-context:" + encoder.ffmpegName, backend);
    if (!emission) return ::media::Status::failure(emission.error());
    encoder.preparedEmission = std::move(emission).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
