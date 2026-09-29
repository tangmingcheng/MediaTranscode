#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"
extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
::media::Status MediaVideoCanvasGeometryValidator::validate(const MediaVideoCanvasGeometry& geometry)
{
    const auto invalid = [](const char* message) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
    };
    const auto* hardware = av_pix_fmt_desc_get(geometry.hardwareFormat);
    const auto* descriptor = av_pix_fmt_desc_get(geometry.softwareFormat);
    if (geometry.width <= 0 || geometry.height <= 0 || geometry.tiles.empty() ||
        (geometry.effectiveColorRange.range != AVCOL_RANGE_MPEG &&
         geometry.effectiveColorRange.range != AVCOL_RANGE_JPEG) ||
        !hardware || !(hardware->flags & AV_PIX_FMT_FLAG_HWACCEL) ||
        !descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL))
        return invalid("Canvas geometry requires positive dimensions, explicit range and hardware/software formats");
    for (const auto& tile : geometry.tiles) {
        if (tile.x < 0 || tile.y < 0 || tile.width <= 0 || tile.height <= 0 ||
            tile.width > geometry.width || tile.height > geometry.height ||
            tile.x > geometry.width - tile.width || tile.y > geometry.height - tile.height ||
            tile.x % (1 << descriptor->log2_chroma_w) || tile.y % (1 << descriptor->log2_chroma_h) ||
            tile.width % (1 << descriptor->log2_chroma_w) || tile.height % (1 << descriptor->log2_chroma_h))
            return invalid("Canvas tile rectangle is outside the planned chroma grid");
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
