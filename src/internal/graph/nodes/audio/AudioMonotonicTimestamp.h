#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

extern "C" {
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

::media::Result<int64_t> monotonicAudioFrameTimestamp(int64_t sourcePts,
                                                      AVRational sourceTimeBase,
                                                      AVRational targetTimeBase,
                                                      int64_t nextExpectedPts);

::media::Result<int64_t> nextAudioFrameTimestamp(int64_t framePts,
                                                 int64_t frameSamples);

} // namespace media::ffmpeg::graph
