#include "internal/graph/runtime/zerocopy/MediaHardwareFrameInterop.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaHardwareFrameInterop::apply(const MediaBufferRef& input,
                                                                  const MediaZeroCopyPlan& plan)
{
    if (!input) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("MediaHardwareFrameInterop apply failed: input is null"));
    }

    if (plan.steps.empty()) {
        return ::media::Result<MediaBufferRef>::success(input);
    }

    if (plan.zeroCopy) {
        return ::media::Result<MediaBufferRef>::success(input);
    }

    if (plan.softwareFallback) {
        return ::media::Result<MediaBufferRef>::success(input);
    }

    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::unsupported("MediaHardwareFrameInterop apply failed: unsupported zero-copy plan"));
}

} // namespace media::ffmpeg::graph
