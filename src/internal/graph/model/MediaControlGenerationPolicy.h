#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaControlGenerationPolicy : std::uint8_t {
    OptionalExactWhenPresent = 0,
    RequiredExact = 1
};

} // namespace media::ffmpeg::graph
