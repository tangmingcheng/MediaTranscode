#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>

namespace media::ffmpeg::graph {

struct MediaQueueMetrics {
    std::atomic_uint64_t pushed{ 0 };
    std::atomic_uint64_t popped{ 0 };
    std::atomic_uint64_t dropped{ 0 };
    std::atomic_uint64_t blockedPushes{ 0 };
    std::atomic_uint64_t failedPushes{ 0 };
    std::atomic_uint64_t failedPops{ 0 };
    std::atomic_size_t currentSize{ 0 };
    std::atomic_size_t peakSize{ 0 };

    MediaQueueMetrics() = default;
    MediaQueueMetrics(const MediaQueueMetrics& other) { *this = other; }
    MediaQueueMetrics& operator=(const MediaQueueMetrics& other) noexcept
    {
        pushed = other.pushed.load(); popped = other.popped.load(); dropped = other.dropped.load();
        blockedPushes = other.blockedPushes.load(); failedPushes = other.failedPushes.load();
        failedPops = other.failedPops.load(); currentSize = other.currentSize.load(); peakSize = other.peakSize.load();
        return *this;
    }
};

} // namespace media::ffmpeg::graph
