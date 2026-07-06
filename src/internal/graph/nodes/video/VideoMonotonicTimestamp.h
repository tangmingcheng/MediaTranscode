#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

extern "C" {
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

::media::Result<int64_t> rescaleStrictlyIncreasingTimestamp(int64_t pts,
                                                            AVRational sourceTimeBase,
                                                            AVRational targetTimeBase,
                                                            int64_t lastSubmittedPts);

} // namespace media::ffmpeg::graph
