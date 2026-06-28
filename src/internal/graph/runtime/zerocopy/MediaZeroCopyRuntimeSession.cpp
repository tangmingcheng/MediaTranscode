#include "internal/graph/runtime/zerocopy/MediaZeroCopyRuntimeSession.h"

#include "internal/graph/runtime/zerocopy/MediaHardwareFrameInterop.h"

namespace media::ffmpeg::graph {

void MediaZeroCopyRuntimeSession::setPolicy(MediaZeroCopyPolicy policy) noexcept
{
    m_policy = policy;
}

const MediaZeroCopyPolicy& MediaZeroCopyRuntimeSession::policy() const noexcept
{
    return m_policy;
}

::media::Result<MediaBufferRef> MediaZeroCopyRuntimeSession::process(const MediaBufferRef& input,
                                                                      const MediaZeroCopyPlan& plan)
{
    if (!m_policy.enabled()) {
        return ::media::Result<MediaBufferRef>::success(input);
    }

    if (m_policy.required() && !plan.zeroCopy) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::unsupported("MediaZeroCopyRuntimeSession failed: plan cannot preserve zero-copy path"));
    }

    return MediaHardwareFrameInterop::apply(input, plan);
}

} // namespace media::ffmpeg::graph
