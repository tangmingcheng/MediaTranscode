#include "internal/graph/time/MediaSteadyMasterClock.h"

namespace media::ffmpeg::graph {

MediaSteadyMasterClock::MediaSteadyMasterClock(
    MediaRunningTime masterAtAnchor) noexcept
    : m_masterAtAnchor(masterAtAnchor)
    , m_steadyAnchor(std::chrono::steady_clock::now())
{
}

::media::Result<MediaRunningTime> MediaSteadyMasterClock::now() const noexcept
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_steadyAnchor);
    return m_masterAtAnchor.checkedAdd(
        MediaRunningTime::fromNanoseconds(elapsed.count()));
}

} // namespace media::ffmpeg::graph
