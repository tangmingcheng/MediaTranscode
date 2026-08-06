#pragma once

#include <chrono>
#include <cstdint>

namespace media::ffmpeg::graph {

inline std::int64_t mediaSteadyClockNowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace media::ffmpeg::graph
