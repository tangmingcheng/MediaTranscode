#pragma once

#include "internal/graph/runtime/queue/MediaQueueMetrics.h"

#include <cstdint>
#include <atomic>

namespace media::ffmpeg::graph {

struct MediaChannelMetrics {
    std::atomic_uint64_t pushed{ 0 };
    std::atomic_uint64_t popped{ 0 };
    std::atomic_uint64_t closed{ 0 };
    std::atomic_uint64_t aborted{ 0 };
    std::atomic_uint64_t cleared{ 0 };

    MediaQueueMetrics queue;

    MediaChannelMetrics() = default;
    MediaChannelMetrics(const MediaChannelMetrics& other) { *this = other; }
    MediaChannelMetrics& operator=(const MediaChannelMetrics& other) noexcept
    {
        pushed = other.pushed.load(); popped = other.popped.load(); closed = other.closed.load();
        aborted = other.aborted.load(); cleared = other.cleared.load(); queue = other.queue;
        return *this;
    }
};

} // namespace media::ffmpeg::graph
