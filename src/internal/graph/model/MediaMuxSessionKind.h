#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaMuxSessionKind : std::uint8_t {
    FFmpegFile = 0,
    ProjectMpegTs = 1
};

::media::Result<std::string> mediaMuxSessionKindOptionValue(
    MediaMuxSessionKind kind);
::media::Result<MediaMuxSessionKind> parseMediaMuxSessionKindOption(
    std::string_view value);

} // namespace media::ffmpeg::graph
