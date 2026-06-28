#pragma once

#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/queue/MediaQueue.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaQueueFactory final {
public:
    static std::unique_ptr<MediaQueue> create(const MediaQueuePolicy& policy);
};

} // namespace media::ffmpeg::graph
