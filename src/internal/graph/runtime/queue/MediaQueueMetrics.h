#pragma once

#include <cstdint>
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaQueueMetrics {
    uint64_t pushed = 0;
    uint64_t popped = 0;
    uint64_t dropped = 0;
    uint64_t blockedPushes = 0;
    uint64_t failedPushes = 0;
    uint64_t failedPops = 0;

    std::size_t currentSize = 0;
    std::size_t peakSize = 0;
};

} // namespace media::ffmpeg::graph
