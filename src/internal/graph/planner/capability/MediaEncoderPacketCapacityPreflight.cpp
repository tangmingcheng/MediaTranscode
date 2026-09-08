#include "internal/graph/planner/capability/MediaEncoderPacketCapacityPreflight.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <algorithm>
#include <limits>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

constexpr int RockchipMppPacketBufferAlignment = 64;

::media::Result<int> alignedRockchipExtent(int value, const char* field)
{
    if (value <= 0 ||
        value > (std::numeric_limits<int>::max)() -
                    (RockchipMppPacketBufferAlignment - 1)) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("RKMPP encoder ") + field +
                " is outside the packet-capacity range"));
    }
    const int aligned =
        (value + RockchipMppPacketBufferAlignment - 1) /
        RockchipMppPacketBufferAlignment *
        RockchipMppPacketBufferAlignment;
    return ::media::Result<int>::success(aligned);
}

AVPixelFormat encoderSoftwareSurface(const AVCodecContext& context) noexcept
{
    if (context.hw_frames_ctx && context.hw_frames_ctx->data) {
        const auto* frames = reinterpret_cast<const AVHWFramesContext*>(
            context.hw_frames_ctx->data);
        if (frames->sw_format != AV_PIX_FMT_NONE) return frames->sw_format;
    }
    if (context.sw_pix_fmt != AV_PIX_FMT_NONE) return context.sw_pix_fmt;
    const auto* descriptor = av_pix_fmt_desc_get(context.pix_fmt);
    if (descriptor && !(descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        return context.pix_fmt;
    }
    return AV_PIX_FMT_NONE;
}

::media::Result<MediaEncoderPacketCapacityPreflightResult>
deriveRockchipMppCapacity(
    const AVCodecContext& context,
    std::uint64_t rateControlBurstBytes)
{
    using Result =
        ::media::Result<MediaEncoderPacketCapacityPreflightResult>;
    const auto format = encoderSoftwareSurface(context);
    auto width = alignedRockchipExtent(context.width, "width");
    auto height = alignedRockchipExtent(context.height, "height");
    if (format == AV_PIX_FMT_NONE || !width || !height) {
        return Result::failure(
            format == AV_PIX_FMT_NONE
                ? ::media::ErrorInfo::notInitialized(
                      "RKMPP encoder did not expose its software surface format")
                : (!width ? width.error() : height.error()));
    }
    const int packetBufferBytes = av_image_get_buffer_size(
        format, width.value(), height.value(), 1);
    if (packetBufferBytes <= 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RKMPP encoder surface cannot form the MPP packet-buffer capacity"));
    }
    return Result::success(MediaEncoderPacketCapacityPreflightResult{
        (std::max)(rateControlBurstBytes,
                   static_cast<std::uint64_t>(packetBufferBytes)),
        "rockchip-mpp:mpi_enc_utils-frame_size-packet-capacity"});
}

::media::Result<MediaEncoderPacketCapacityPreflightResult>
deriveNvencCapacity(
    const AVCodecContext& context,
    std::uint64_t rateControlBurstBytes)
{
    using Result =
        ::media::Result<MediaEncoderPacketCapacityPreflightResult>;
    const auto format = encoderSoftwareSurface(context);
    if (format == AV_PIX_FMT_NONE || context.width <= 0 ||
        context.height <= 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "NVENC encoder did not expose its input YUV geometry"));
    }
    const int inputYuvBytes = av_image_get_buffer_size(
        format, context.width, context.height, 1);
    if (inputYuvBytes <= 0 ||
        static_cast<std::uint64_t>(inputYuvBytes) >
            (std::numeric_limits<std::uint64_t>::max)() / 2U) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "NVENC input YUV geometry cannot form its output buffer capacity"));
    }
    const auto recommendedOutputBytes =
        static_cast<std::uint64_t>(inputYuvBytes) * 2U;
    return Result::success(MediaEncoderPacketCapacityPreflightResult{
        (std::max)(rateControlBurstBytes, recommendedOutputBytes),
        "nvidia-video-codec-sdk:recommended-output-buffer=2x-input-yuv"});
}

} // namespace

::media::Result<MediaEncoderPacketCapacityPreflightResult>
MediaEncoderPacketCapacityPreflight::derive(
    const AVCodecContext& context,
    const std::string& backend,
    std::uint64_t rateControlBurstBytes)
{
    using Result =
        ::media::Result<MediaEncoderPacketCapacityPreflightResult>;
    if (rateControlBurstBytes == 0 || backend.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "encoder packet-capacity preflight requires backend and rate-control evidence"));
    }
    if (backend == "rkmpp") {
        return deriveRockchipMppCapacity(context, rateControlBurstBytes);
    }
    if (context.codec && context.codec->name &&
        std::string_view(context.codec->name).ends_with("_nvenc")) {
        return deriveNvencCapacity(context, rateControlBurstBytes);
    }
    return Result::success(MediaEncoderPacketCapacityPreflightResult{
        rateControlBurstBytes,
        "opened-encoder-rate-control-burst"});
}

} // namespace media::ffmpeg::graph
