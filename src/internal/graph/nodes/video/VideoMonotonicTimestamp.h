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

::media::Result<int64_t> syntheticTimestampStep(AVRational frameStepTimeBase,
                                                AVRational timestampTimeBase);

::media::Result<int64_t> nextSyntheticTimestamp(int64_t lastSubmittedPts,
                                                AVRational frameStepTimeBase,
                                                AVRational timestampTimeBase);

} // namespace media::ffmpeg::graph
