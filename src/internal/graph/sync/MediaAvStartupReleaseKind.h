#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvStartupReleaseKind : std::uint8_t {
    InitialAtomicRelease = 0,
    ActiveEpochPassThrough = 1,
    NextAtomicRelease = 2
};

enum class MediaAvStartupReleaseDisposition : std::uint8_t {
    Publish = 0,
    DropOld = 1,
    Withhold = 2,
    Reject = 3
};

} // namespace media::ffmpeg::graph
