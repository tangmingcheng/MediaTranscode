#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <limits>

namespace media::ffmpeg::graph {
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num > 0 && rational.den > 0;
}

} // namespace

::media::Result<int64_t> rescaleStrictlyIncreasingTimestamp(int64_t pts,
                                                            AVRational sourceTimeBase,
                                                            AVRational targetTimeBase,
                                                            int64_t lastSubmittedPts)
{
    if (pts == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp requires valid pts"));
    }

    if (!rationalKnown(sourceTimeBase) || !rationalKnown(targetTimeBase)) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp requires valid time_base"));
    }

    const int64_t rescaled = av_rescale_q(pts, sourceTimeBase, targetTimeBase);
    if (rescaled == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp rescaled pts is invalid"));
    }

    if (lastSubmittedPts != AV_NOPTS_VALUE && rescaled <= lastSubmittedPts) {
        if (lastSubmittedPts == std::numeric_limits<int64_t>::max()) {
            return ::media::Result<int64_t>::failure(
                ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp cannot advance past int64 max"));
        }
        return ::media::Result<int64_t>::success(lastSubmittedPts + 1);
    }
    return ::media::Result<int64_t>::success(rescaled);
}

} // namespace media::ffmpeg::graph
