#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// Arrival evidence is independent of decoded/encoded progress. Like WebRTC's
// LastReceivedPacketMs, it uses receive time, not time spent draining a queue.
class MediaInputActivity final {
public:
    void observe(std::int64_t receivedAtNanoseconds) noexcept
    {
        auto previous = m_lastReceivedAtNanoseconds.load(std::memory_order_relaxed);
        while (receivedAtNanoseconds > previous &&
               !m_lastReceivedAtNanoseconds.compare_exchange_weak(
                   previous, receivedAtNanoseconds, std::memory_order_relaxed)) {
        }
    }

    std::optional<std::int64_t> lastReceivedAtNanoseconds() const noexcept
    {
        const auto value = m_lastReceivedAtNanoseconds.load(std::memory_order_relaxed);
        return value > 0 ? std::optional<std::int64_t>(value) : std::nullopt;
    }

private:
    std::atomic<std::int64_t> m_lastReceivedAtNanoseconds{0};
};

} // namespace media::ffmpeg::graph
