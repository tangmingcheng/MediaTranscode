#include "internal/graph/runtime/lifecycle/MediaRealtimeProgressTracker.h"

namespace media::ffmpeg::graph {

::media::Result<bool> MediaRealtimeProgressTracker::observe(
    std::uint64_t workerProgress,
    std::uint64_t encodedPacketsPushed,
    std::chrono::milliseconds elapsedSinceStart) noexcept
{
    if (workerProgress < m_workerProgress ||
        encodedPacketsPushed < m_encodedPacketsPushed ||
        elapsedSinceStart < m_elapsedSinceStart) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime progress observations must be monotonic"));
    }

    const bool encodedProgress =
        encodedPacketsPushed > m_encodedPacketsPushed;
    const bool startupProgress =
        !m_outputStarted && workerProgress > m_workerProgress;
    if (encodedProgress && !m_outputStarted) {
        m_firstOutputElapsedSinceStart = elapsedSinceStart;
        m_outputStarted = true;
    }
    m_workerProgress = workerProgress;
    m_encodedPacketsPushed = encodedPacketsPushed;
    m_elapsedSinceStart = elapsedSinceStart;
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
    if (!m_outputStarted) {
        return elapsedSinceStart >= startupDeadline;
    }
    return m_firstOutputElapsedSinceStart >= startupDeadline;
}

bool MediaRealtimeProgressTracker::maximumOutputDurationExpired(
    std::chrono::milliseconds elapsedSinceStart,
    std::chrono::milliseconds maximumOutputDuration) const noexcept
{
    return m_outputStarted &&
           elapsedSinceStart >= m_firstOutputElapsedSinceStart &&
           elapsedSinceStart - m_firstOutputElapsedSinceStart >=
               maximumOutputDuration;
}

} // namespace media::ffmpeg::graph
