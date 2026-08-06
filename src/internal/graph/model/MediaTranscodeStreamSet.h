#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaTranscodeStreamSet : std::uint8_t {
    AudioVideo = 0,
    VideoOnly = 1,
};

} // namespace media::ffmpeg::graph
