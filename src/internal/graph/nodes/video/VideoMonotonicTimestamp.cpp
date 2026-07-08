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

::media::Result<int64_t> syntheticTimestampStep(AVRational frameStepTimeBase,
                                                AVRational timestampTimeBase)
{
    if (!rationalKnown(frameStepTimeBase) || !rationalKnown(timestampTimeBase)) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp requires valid synthetic time_base"));
    }

    int64_t step = av_rescale_q(1, frameStepTimeBase, timestampTimeBase);
    if (step == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp synthetic step is invalid"));
    }
    if (step <= 0) {
        step = 1;
    }
    return ::media::Result<int64_t>::success(step);
}

::media::Result<int64_t> nextSyntheticTimestamp(int64_t lastSubmittedPts,
                                                AVRational frameStepTimeBase,
                                                AVRational timestampTimeBase)
{
    auto step = syntheticTimestampStep(frameStepTimeBase, timestampTimeBase);
    if (!step) {
        return ::media::Result<int64_t>::failure(step.error());
    }
    if (lastSubmittedPts == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::success(0);
    }
    if (lastSubmittedPts > std::numeric_limits<int64_t>::max() - step.value()) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("VideoMonotonicTimestamp cannot synthesize past int64 max"));
    }
    return ::media::Result<int64_t>::success(lastSubmittedPts + step.value());
}

} // namespace media::ffmpeg::graph
