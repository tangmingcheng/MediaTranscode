#pragma once

#include <cstdint>

extern "C" {
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

int64_t rescaleStrictlyIncreasingTimestamp(int64_t pts,
                                           AVRational sourceTimeBase,
                                           AVRational targetTimeBase,
                                           int64_t lastSubmittedPts) noexcept;

} // namespace media::ffmpeg::graph
