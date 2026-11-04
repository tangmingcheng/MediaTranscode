#include "internal/graph/planner/video/MediaVideoColorContractPlanner.h"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
}
namespace media::ffmpeg::graph {
::media::Status MediaVideoColorContractPlanner::validate(const MediaVideoColorContract& color)
{
    if ((color.range != AVCOL_RANGE_MPEG && color.range != AVCOL_RANGE_JPEG) ||
        color.primaries == AVCOL_PRI_RESERVED0 ||
        color.primaries == AVCOL_PRI_RESERVED || !av_color_primaries_name(color.primaries) ||
        color.transfer == AVCOL_TRC_RESERVED0 ||
        color.transfer == AVCOL_TRC_RESERVED || !av_color_transfer_name(color.transfer) ||
        color.matrix == AVCOL_SPC_RESERVED ||
        !av_color_space_name(color.matrix))
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Composition color contract contains an invalid range or reserved colorimetry value"));
    return ::media::Status::success();
}
::media::Result<MediaVideoColorContract> MediaVideoColorContractPlanner::plan(
    std::span<const AVFrame* const> decoded, std::span<const AVFrame* const> tiles,
    std::span<const MediaVideoColorRangeFact> signaling)
{
    using Result = ::media::Result<MediaVideoColorContract>;
    if (decoded.empty() || decoded.size() != tiles.size() || decoded.size() != signaling.size())
        return Result::failure(::media::ErrorInfo::invalidArgument("Composition color evidence count differs"));
    std::optional<MediaVideoColorContract> common;
    std::optional<AVPixelFormat> commonFormat;
    const auto surfaceFormat = [](const AVFrame& frame) {
        if (frame.hw_frames_ctx && frame.hw_frames_ctx->data)
            return reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data)->sw_format;
        return static_cast<AVPixelFormat>(frame.format);
    };
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        if (!decoded[i] || !tiles[i]) return Result::failure(
            ::media::ErrorInfo::notInitialized("Composition actual decoded frame or filtered tile is missing"));
        const auto read = [](const AVFrame& frame) {
            return MediaVideoColorContract{frame.color_range, frame.color_primaries, frame.color_trc, frame.colorspace};
        };
        const auto format = surfaceFormat(*decoded[i]);
        if (format == AV_PIX_FMT_NONE || format != surfaceFormat(*tiles[i]) ||
            (commonFormat && format != *commonFormat))
            return Result::failure(::media::ErrorInfo::unsupported(
                "Composition requires one pixel domain without unplanned pixel-format/color conversion"));
        commonFormat = format;
        const auto color = read(*decoded[i]);
        if (auto status = validate(color); !status) return Result::failure(status.error());
        if (color.range != signaling[i].range || (signaling[i].color && color != *signaling[i].color) ||
            color != read(*tiles[i]) || (common && color != *common))
            return Result::failure(::media::ErrorInfo::unsupported(
                "Composition decoded/SPS/tile colorimetry differs; no color conversion contract is planned"));
        common = color;
    }
    return Result::success(*common);
}
} // namespace media::ffmpeg::graph
