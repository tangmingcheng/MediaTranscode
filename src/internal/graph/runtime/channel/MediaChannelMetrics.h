#pragma once

#include "internal/graph/runtime/queue/MediaQueueMetrics.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaChannelMetrics {
    uint64_t pushed = 0;
    uint64_t popped = 0;
    uint64_t closed = 0;
    uint64_t aborted = 0;
    uint64_t cleared = 0;

    MediaQueueMetrics queue;
};

} // namespace media::ffmpeg::graph
