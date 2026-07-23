#include "internal/graph/runtime/lifecycle/MediaRealtimeProgressTracker.h"

namespace media::ffmpeg::graph {

::media::Result<bool> MediaRealtimeProgressTracker::observe(
    std::uint64_t workerProgress,
    std::uint64_t encodedPacketsPushed) noexcept
{
    if (workerProgress < m_workerProgress ||
        encodedPacketsPushed < m_encodedPacketsPushed) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime progress counters must be monotonic"));
    }

    const bool encodedProgress =
        encodedPacketsPushed > m_encodedPacketsPushed;
    const bool startupProgress =
        !m_outputStarted && workerProgress > m_workerProgress;
    if (encodedProgress) {
        m_outputStarted = true;
    }
    m_workerProgress = workerProgress;
    m_encodedPacketsPushed = encodedPacketsPushed;
    return ::media::Result<bool>::success(
        encodedProgress || startupProgress);
}

bool MediaRealtimeProgressTracker::outputStarted() const noexcept
{
    return m_outputStarted;
}

bool MediaRealtimeProgressTracker::firstOutputDeadlineExpired(
    std::chrono::milliseconds elapsedSinceStart,
    std::chrono::milliseconds startupDeadline) const noexcept
{
    return !m_outputStarted && elapsedSinceStart >= startupDeadline;
}

} // namespace media::ffmpeg::graph
