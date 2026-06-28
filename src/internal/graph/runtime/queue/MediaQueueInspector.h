#pragma once

#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/queue/MediaQueue.h"
#include "internal/graph/runtime/queue/MediaQueueMetrics.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

struct MediaQueueSnapshot {
    MediaQueuePolicy policy;
    MediaQueueMetrics metrics;
    std::size_t size = 0;
    std::size_t capacity = 0;
    bool closed = false;
    bool aborted = false;
    bool saturated = false;
    bool healthy = true;
    std::string summary;
};

class MediaQueueInspector final {
public:
    static MediaQueueSnapshot inspect(const MediaQueue& queue);
};

} // namespace media::ffmpeg::graph
