#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"

namespace media::ffmpeg::graph {

MediaInitialClockAcquisitionDeadline::MediaInitialClockAcquisitionDeadline(
    MediaRunningTime plannedTimeout) noexcept
    : m_plannedTimeout(plannedTimeout)
{
}

::media::Result<MediaInitialClockAcquisitionDeadline>
MediaInitialClockAcquisitionDeadline::create(MediaRunningTime plannedTimeout)
{
    if (plannedTimeout <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaInitialClockAcquisitionDeadline>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Initial clock acquisition timeout must be positive"));
    }
    return ::media::Result<MediaInitialClockAcquisitionDeadline>::success(
        MediaInitialClockAcquisitionDeadline(plannedTimeout));
}

::media::Status MediaInitialClockAcquisitionDeadline::establish(
    MediaRunningTime masterNow)
{
    if (m_deadline) return ::media::Status::success();
    auto deadline = masterNow.checkedAdd(m_plannedTimeout);
    if (!deadline) return ::media::Status::failure(deadline.error());
    m_deadline = deadline.value();
    return ::media::Status::success();
}

::media::Status MediaInitialClockAcquisitionDeadline::preflight(
    MediaRunningTime masterNow) const
{
    if (m_deadline && masterNow >= *m_deadline) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Initial clock acquisition master deadline expired"));
    }
    return ::media::Status::success();
}

const std::optional<MediaRunningTime>&
MediaInitialClockAcquisitionDeadline::deadline() const noexcept
{
    return m_deadline;
}

void MediaInitialClockAcquisitionDeadline::clear() noexcept
{
    m_deadline.reset();
}

} // namespace media::ffmpeg::graph
