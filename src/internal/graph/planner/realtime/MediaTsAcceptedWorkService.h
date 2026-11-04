#pragma once
#include "internal/graph/time/MediaRunningTime.h"
#include <cstddef>

namespace media::ffmpeg::graph {
// Peak-rate service projection with conservative possible-report allowance.
// Neither a submission observation nor a guarantee of service availability.
struct MediaTsPublishedWorkService final {
    MediaRunningTime availableAt;
};

struct MediaTsUnpublishedTail final {
    MediaRunningTime release;
    MediaRunningTime deadline;
    std::size_t payloadBytes;
    std::size_t datagramCount;
};
} // namespace media::ffmpeg::graph
