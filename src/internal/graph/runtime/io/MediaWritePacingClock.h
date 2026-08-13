#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaWritePacingClock final {
public:
    MediaWritePacingClock(
        std::int64_t bytesPerSecond,
        std::int64_t burstBytes) noexcept;

    void waitFor(std::size_t bytes) noexcept;
    void reset() noexcept;

private:
    using Clock = std::chrono::steady_clock;

    void refill(Clock::time_point now, long double capacity) noexcept;

    std::int64_t m_bytesPerSecond;
    std::int64_t m_burstBytes;
    long double m_availableBytes;
    Clock::time_point m_lastRefill;
};

} // namespace media::ffmpeg::graph
