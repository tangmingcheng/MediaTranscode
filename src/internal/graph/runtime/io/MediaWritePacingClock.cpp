#include "internal/graph/runtime/io/MediaWritePacingClock.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace media::ffmpeg::graph {

MediaWritePacingClock::MediaWritePacingClock(
    std::int64_t bytesPerSecond,
    std::int64_t burstBytes) noexcept
    : m_bytesPerSecond(bytesPerSecond),
      m_burstBytes(burstBytes),
      m_availableBytes(static_cast<long double>(burstBytes)),
      m_lastRefill(Clock::now())
{
}

void MediaWritePacingClock::refill(
    Clock::time_point now,
    long double capacity) noexcept
{
    const auto elapsed = now - m_lastRefill;
    if (elapsed <= Clock::duration::zero()) return;
    const long double elapsedSeconds =
        std::chrono::duration<long double>(elapsed).count();
    m_availableBytes = std::min<long double>(
        capacity,
        m_availableBytes + elapsedSeconds *
            static_cast<long double>(m_bytesPerSecond));
    m_lastRefill = now;
}

void MediaWritePacingClock::waitFor(std::size_t bytes) noexcept
{
    if (bytes == 0 || m_bytesPerSecond <= 0 || m_burstBytes <= 0) return;
    const long double required = static_cast<long double>(bytes);
    const long double capacity = std::max<long double>(
        static_cast<long double>(m_burstBytes), required);
    for (;;) {
        const auto now = Clock::now();
        refill(now, capacity);
        if (m_availableBytes >= required) {
            m_availableBytes -= required;
            return;
        }
        const long double missing = required - m_availableBytes;
        const auto wait = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<long double>(
                missing / static_cast<long double>(m_bytesPerSecond)));
        std::this_thread::sleep_until(
            now + std::max(wait, Clock::duration{1}));
    }
}

void MediaWritePacingClock::reset() noexcept
{
    m_availableBytes = static_cast<long double>(m_burstBytes);
    m_lastRefill = Clock::now();
}

} // namespace media::ffmpeg::graph
