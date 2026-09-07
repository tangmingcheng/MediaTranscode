#pragma once

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpAccessUnitContinuity final {
public:
    void markLost() noexcept
    {
        m_waitingForBoundary = true;
        m_unprovenTimestamp.reset();
    }

    bool accept(std::uint32_t timestamp) noexcept
    {
        if (!m_waitingForBoundary) return true;
        if (!m_unprovenTimestamp) m_unprovenTimestamp = timestamp;
        if (*m_unprovenTimestamp == timestamp) return false;
        // After a sequence gap the first resumed AU may have lost its prefix.
        // Only a subsequent, continuously received timestamp proves a boundary.
        m_waitingForBoundary = false;
        m_unprovenTimestamp.reset();
        return true;
    }

private:
    bool m_waitingForBoundary = false;
    std::optional<std::uint32_t> m_unprovenTimestamp;
};

} // namespace media::ffmpeg::graph
