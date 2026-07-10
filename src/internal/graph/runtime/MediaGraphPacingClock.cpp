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
    if (!m_policy.enablePacing || !timeBase.isKnown()) {
        return ::media::Status::success();
    }

    if (m_start == Clock::time_point{}) {
        reset();
    }
    if (!m_basePts.has_value()) {
        m_basePts = pts;
    }

    const auto target = m_start + toMicroseconds(pts - *m_basePts, timeBase);
    const auto now = Clock::now();

    if (target > now) {
        std::this_thread::sleep_until(target);
    }

    return ::media::Status::success();
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
