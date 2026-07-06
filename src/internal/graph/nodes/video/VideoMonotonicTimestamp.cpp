#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg::graph {

int64_t rescaleStrictlyIncreasingTimestamp(int64_t pts,
                                           AVRational sourceTimeBase,
                                           AVRational targetTimeBase,
                                           int64_t lastSubmittedPts) noexcept
{
    const int64_t rescaled = av_rescale_q(pts, sourceTimeBase, targetTimeBase);
    if (lastSubmittedPts != AV_NOPTS_VALUE && rescaled <= lastSubmittedPts) {
        return lastSubmittedPts + 1;
    }
    return rescaled;
}

} // namespace media::ffmpeg::graph
