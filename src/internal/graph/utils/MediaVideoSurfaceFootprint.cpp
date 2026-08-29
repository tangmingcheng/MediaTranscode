#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t> MediaVideoSurfaceFootprint::logicalBytes(
    int width,
    int height,
    const std::string& pixelFormat)
{
    if (width <= 0 || height <= 0 || pixelFormat.empty()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "video surface footprint requires prepared geometry and pixel format"));
    }
    const AVPixelFormat format = av_get_pix_fmt(pixelFormat.c_str());
    if (format == AV_PIX_FMT_NONE) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "video surface footprint rejects an unknown FFmpeg pixel format: " +
                pixelFormat));
    }
    const int bytes = av_image_get_buffer_size(format, width, height, 1);
    if (bytes <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg cannot represent the prepared video surface footprint"));
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(bytes));
}

} // namespace media::ffmpeg::graph
