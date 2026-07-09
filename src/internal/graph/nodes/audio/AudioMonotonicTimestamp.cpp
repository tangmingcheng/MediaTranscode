#include "internal/graph/nodes/audio/AudioMonotonicTimestamp.h"

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

::media::Result<int64_t> monotonicAudioFrameTimestamp(int64_t sourcePts,
                                                      AVRational sourceTimeBase,
                                                      AVRational targetTimeBase,
                                                      int64_t nextExpectedPts)
{
    if (sourcePts == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp requires valid pts"));
    }
    if (!rationalKnown(sourceTimeBase) || !rationalKnown(targetTimeBase)) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp requires valid time_base"));
    }

    const int64_t rescaled = av_rescale_q(sourcePts, sourceTimeBase, targetTimeBase);
    if (rescaled == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp rescaled pts is invalid"));
    }
    if (nextExpectedPts != AV_NOPTS_VALUE && rescaled < nextExpectedPts) {
        return ::media::Result<int64_t>::success(nextExpectedPts);
    }
    return ::media::Result<int64_t>::success(rescaled);
}

::media::Result<int64_t> nextAudioFrameTimestamp(int64_t framePts,
                                                 int64_t frameSamples)
{
    if (framePts == AV_NOPTS_VALUE) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp requires valid frame pts"));
    }
    if (frameSamples <= 0) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp requires positive frame samples"));
    }
    if (framePts > std::numeric_limits<int64_t>::max() - frameSamples) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("AudioMonotonicTimestamp cannot advance past int64 max"));
    }
    return ::media::Result<int64_t>::success(framePts + frameSamples);
}

} // namespace media::ffmpeg::graph
