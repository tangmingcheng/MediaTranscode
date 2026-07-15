#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaOutputResourceKind : std::uint8_t {
    FFmpegFormatContext = 0,
    ByteSink = 1
};

::media::Result<std::string> mediaOutputResourceKindOptionValue(
    MediaOutputResourceKind kind);
::media::Result<MediaOutputResourceKind> parseMediaOutputResourceKindOption(
    std::string_view value);

} // namespace media::ffmpeg::graph
