#pragma once

#include "media_transcode/Result.h"

#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaRtcpCompositionMode {
    StrictCompoundRfc3550,
    ReducedSizeRfc5506
};

struct MediaRtcpCompoundPolicy final {
    MediaRtcpCompositionMode mode;
    bool requireCname;
};

::media::Result<std::string> serializeMediaRtcpCompositionMode(
    MediaRtcpCompositionMode mode);
::media::Result<MediaRtcpCompositionMode> parseMediaRtcpCompositionMode(
    std::string_view value);

} // namespace media::ffmpeg::graph
