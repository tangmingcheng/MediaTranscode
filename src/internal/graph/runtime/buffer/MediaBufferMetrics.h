#pragma once

#include "internal/graph/model/MediaGraphTypes.h"

#include <cstdint>
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaBufferMetrics {
    uint64_t allocatedBuffers = 0;
    uint64_t reusedBuffers = 0;
    uint64_t releasedBuffers = 0;
    uint64_t droppedBuffers = 0;

    MediaByteSize allocatedBytes = 0;
    std::size_t pooledBuffers = 0;
};

} // namespace media::ffmpeg::graph
