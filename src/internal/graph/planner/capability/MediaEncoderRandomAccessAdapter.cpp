#include "internal/graph/planner/capability/MediaEncoderRandomAccessAdapter.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
}
#include <string_view>

namespace media::ffmpeg::graph {
::media::Result<std::optional<MediaPreparedVideoRandomAccessEnvelope>>
MediaEncoderRandomAccessAdapter::readAfterOpen(AVCodecContext& encoder)
{
    using Result = ::media::Result<std::optional<MediaPreparedVideoRandomAccessEnvelope>>;
    if (!encoder.codec || !avcodec_is_open(&encoder)) return Result::failure(
        ::media::ErrorInfo::notInitialized("Random access evidence requires an opened encoder"));
    const std::string_view name(encoder.codec->name);
    if (name == "h264_rkmpp" || name == "hevc_rkmpp") {
        std::int64_t refresh = 0;
        std::int64_t refreshRows = 0;
        if (!encoder.priv_data ||
            av_opt_get_int(encoder.priv_data, "intra_refresh", 0, &refresh) < 0 ||
            av_opt_get_int(encoder.priv_data, "refresh_num", 0, &refreshRows) < 0)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "RKMPP random access evidence requires actual intra-refresh option readback"));
        if (refresh != 0 || refreshRows <= 0 || encoder.gop_size <= 0 ||
            encoder.framerate.num <= 0 || encoder.framerate.den <= 0)
            return Result::success(std::nullopt);
        int backendNumerator = 0;
        int backendDenominator = 0;
        // rkmpp_set_enc_cfg reduces rc:fps_in/out with this backend limit.
        // An approximated cadence cannot certify the requested IDR interval.
        if (!av_reduce(&backendNumerator, &backendDenominator,
                       encoder.framerate.num, encoder.framerate.den, 65535))
            return Result::success(std::nullopt);
        // d90e3a1 opens with rc:gop and no custom reference configuration.
        // c08762ebf uses the default temporal-zero reference cycle; igop
        // resets seq_idx to zero when refresh is disabled, producing IDR.
        // H264 maps is_idr to NAL 5; HEVC temporal-zero intra maps to NAL 19.
        return Result::success(MediaPreparedVideoRandomAccessEnvelope{
            static_cast<std::uint64_t>(encoder.gop_size),
            {backendNumerator, backendDenominator}, true,
            "ffmpeg-rockchip-d90e3a1+mpp-c08762ebf-rc-gop-default-refs-refresh-disabled"});
    }
    if (name != "h264_nvenc" && name != "hevc_nvenc") return Result::success(std::nullopt);
    std::int64_t refresh = 0;
    std::int64_t singleSliceRefresh = 0;
    if (!encoder.priv_data ||
        av_opt_get_int(encoder.priv_data, "intra-refresh", 0, &refresh) < 0 ||
        av_opt_get_int(encoder.priv_data, "single-slice-intra-refresh", 0, &singleSliceRefresh) < 0)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "NVENC random access evidence requires actual intra-refresh option readback"));
    // Verified FFmpeg 20054712 nvenc.c: explicit positive gop_size becomes
    // gopLength and H264/HEVC idrPeriod; either refresh mode makes it infinite.
    // Unknown preset GOPs and intra-refresh remain valid encoders, but cannot
    // authorize a bounded natural-IDR join.
    if (refresh != 0 || singleSliceRefresh != 0 || encoder.gop_size <= 0 ||
        encoder.framerate.num <= 0 || encoder.framerate.den <= 0)
        return Result::success(std::nullopt);
    return Result::success(MediaPreparedVideoRandomAccessEnvelope{
        static_cast<std::uint64_t>(encoder.gop_size),
        {encoder.framerate.num, encoder.framerate.den}, true,
        "FFmpeg-nvenc-20054712-explicit-gopLength-idrPeriod-refresh-disabled-readback"});
}
} // namespace media::ffmpeg::graph
