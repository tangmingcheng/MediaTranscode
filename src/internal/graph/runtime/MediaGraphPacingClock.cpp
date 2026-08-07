#include "internal/graph/runtime/MediaGraphPacingClock.h"

#include <thread>

namespace media::ffmpeg::graph {

void MediaGraphPacingClock::reset()
{
    m_start = Clock::now();
    m_basePts.reset();
}

void MediaGraphPacingClock::setPolicy(MediaLatencyPolicy policy) noexcept
{
    m_policy = policy;
}

const MediaLatencyPolicy& MediaGraphPacingClock::policy() const noexcept
{
    return m_policy;
}

::media::Status MediaGraphPacingClock::waitUntil(MediaTimeValue pts, MediaRational timeBase)
{
    if (!m_policy.enablePacing) return ::media::Status::success();
    auto target = targetTime(pts, timeBase);
    if (!target) return ::media::Status::failure(target.error());
    const auto now = Clock::now();

    if (target.value() > now) {
        std::this_thread::sleep_until(target.value());
    }

    return ::media::Status::success();
}

::media::Result<MediaGraphPacingClock::Clock::time_point>
MediaGraphPacingClock::targetTime(MediaTimeValue pts,
                                  MediaRational timeBase)
{
    if (!m_policy.enablePacing) {
        return ::media::Result<Clock::time_point>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Pacing clock requires an enabled pacing policy"));
    }
    if (!timeBase.isKnown() || timeBase.num <= 0 || timeBase.den <= 0) {
        return ::media::Result<Clock::time_point>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Pacing clock requires a positive time base"));
    }
    if (m_start == Clock::time_point{}) reset();
    if (!m_basePts.has_value()) m_basePts = pts;

    return ::media::Result<Clock::time_point>::success(
        m_start + toMicroseconds(pts - *m_basePts, timeBase));
}

std::chrono::microseconds MediaGraphPacingClock::toMicroseconds(MediaTimeValue pts, MediaRational timeBase) noexcept
{
    if (!timeBase.isKnown()) {
        return std::chrono::microseconds{ 0 };
    }

    const int64_t us = (pts * static_cast<int64_t>(timeBase.num) * 1000000LL) /
                       static_cast<int64_t>(timeBase.den);
    return std::chrono::microseconds{ us };
}

} // namespace media::ffmpeg::graph
