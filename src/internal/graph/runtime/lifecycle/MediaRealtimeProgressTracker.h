#pragma once

#include "media_transcode/Result.h"

#include <chrono>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRealtimeProgressTracker final {
public:
    ::media::Result<bool> observe(std::uint64_t workerProgress,
                                  std::uint64_t encodedPacketsPushed) noexcept;
    bool outputStarted() const noexcept;
    bool firstOutputDeadlineExpired(
        std::chrono::milliseconds elapsedSinceStart,
        std::chrono::milliseconds startupDeadline) const noexcept;

private:
    std::uint64_t m_workerProgress = 0;
    std::uint64_t m_encodedPacketsPushed = 0;
    bool m_outputStarted = false;
};

} // namespace media::ffmpeg::graph
