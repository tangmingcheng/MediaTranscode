#include "internal/graph/planner/video/MediaVideoCanvasGeometryPlanner.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <algorithm>
#include <cstdint>
extern "C" {
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
::media::Result<MediaVideoCanvasGeometryPlan> MediaVideoCanvasGeometryPlanner::plan(
    std::span<const MediaInputVideoStreamInfo> sources,
    const MediaHardwareDescriptor& output, MediaVideoColorRangeFact effectiveColorRange)
{
    using Result = ::media::Result<MediaVideoCanvasGeometryPlan>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    const auto hardwareFormat = av_get_pix_fmt(output.pixelFormat.c_str());
    const auto softwareFormat = av_get_pix_fmt(output.surfacePixelFormat.c_str());
    const auto* descriptor = av_pix_fmt_desc_get(softwareFormat);
    if (sources.size() < 2 || sources.size() > 4 || !output.isHardwareBacked() ||
        !descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) ||
        output.size.width <= 0 || output.size.height <= 0)
        return invalid("Fixed canvas requires two to four sources and explicit output surface facts");
    const int alignX = 1 << descriptor->log2_chroma_w;
    const int alignY = 1 << descriptor->log2_chroma_h;
    if (output.size.width % alignX || output.size.height % alignY)
        return invalid("Fixed canvas dimensions do not match the output chroma grid");
    const int columns = 2;
    const int rows = sources.size() == 2 ? 1 : 2;
    const int widthUnits = output.size.width / alignX;
    const int heightUnits = output.size.height / alignY;
    if (widthUnits < columns || heightUnits < rows)
        return invalid("Fixed canvas cannot hold a chroma-aligned cell for each source");
    MediaVideoCanvasGeometryPlan result{{hardwareFormat, softwareFormat,
        output.size.width, output.size.height, effectiveColorRange, {}, {1, 1}}, {1, 1}, {}};
    result.cells.reserve(sources.size());
    result.geometry.tiles.reserve(sources.size());
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const auto& source = sources[index];
        if (source.width <= 0 || source.height <= 0 ||
            source.sampleAspectRatio.num <= 0 || source.sampleAspectRatio.den <= 0)
            return invalid("Fixed canvas source requires authoritative dimensions and sample aspect ratio");
        const int column = static_cast<int>(index) % columns;
        const int row = static_cast<int>(index) / columns;
        const int x = static_cast<int>(static_cast<std::int64_t>(widthUnits) * column / columns) * alignX;
        const int y = static_cast<int>(static_cast<std::int64_t>(heightUnits) * row / rows) * alignY;
        const int right = static_cast<int>(static_cast<std::int64_t>(widthUnits) * (column + 1) / columns) * alignX;
        const int bottom = static_cast<int>(static_cast<std::int64_t>(heightUnits) * (row + 1) / rows) * alignY;
        const MediaVideoCanvasRectangle cell{x, y, right - x, bottom - y};
        // FFmpeg scale's decrease/divisible-by algorithm, expressed with the
        // source display ratio for a square-pixel output. av_rescale_rnd keeps
        // the multiply/divide exact without overflowing the intermediate.
        const auto darNumerator = static_cast<std::int64_t>(source.width) * source.sampleAspectRatio.num;
        const auto darDenominator = static_cast<std::int64_t>(source.height) * source.sampleAspectRatio.den;
        const auto fittedWidth = av_rescale_rnd(cell.height, darNumerator, darDenominator, AV_ROUND_NEAR_INF);
        const auto fittedHeight = av_rescale_rnd(cell.width, darDenominator, darNumerator, AV_ROUND_NEAR_INF);
        if (fittedWidth <= 0 || fittedHeight <= 0)
            return invalid("Source display aspect ratio cannot be represented by the canvas");
        const int width = static_cast<int>(std::min<std::int64_t>(cell.width, fittedWidth)) / alignX * alignX;
        const int height = static_cast<int>(std::min<std::int64_t>(cell.height, fittedHeight)) / alignY * alignY;
        if (width <= 0 || height <= 0)
            return invalid("Source display aspect ratio collapses below the output chroma grid");
        result.cells.push_back(cell);
        result.geometry.tiles.push_back({cell.x + (cell.width - width) / (2 * alignX) * alignX,
            cell.y + (cell.height - height) / (2 * alignY) * alignY, width, height});
    }
    if (auto status = MediaVideoCanvasGeometryValidator::validate(result.geometry); !status)
        return Result::failure(status.error());
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
