#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvSyncTopology : std::uint8_t {
    SeparateRtpToSeparateRtp = 0,
    MpegTsToMpegTs = 1
};

} // namespace media::ffmpeg::graph
