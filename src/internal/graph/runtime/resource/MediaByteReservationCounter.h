#pragma once

#include <cstddef>

namespace media::ffmpeg::graph {

// Caller supplies synchronization and error policy. Accounting never changes
// on failure; the fixed capacity is supplied by the owning planning contract.
class MediaByteReservationCounter final {
public:
    explicit MediaByteReservationCounter(std::size_t capacity) noexcept
        : m_capacity(capacity) {}

    bool retain(std::size_t bytes) noexcept
    {
        if (bytes > m_capacity - m_retainedBytes) return false;
        m_retainedBytes += bytes;
        return true;
    }
    bool release(std::size_t bytes) noexcept
    {
        if (bytes > m_retainedBytes) return false;
        m_retainedBytes -= bytes;
        return true;
    }
    std::size_t capacity() const noexcept { return m_capacity; }
    std::size_t retainedBytes() const noexcept { return m_retainedBytes; }

private:
    const std::size_t m_capacity;
    std::size_t m_retainedBytes = 0;
};

} // namespace media::ffmpeg::graph
