#pragma once

#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaBlockingEdgePolicyPlanner final {
public:
    static MediaEdgePolicy planQueue(std::size_t capacity) noexcept;
    static MediaEdgePolicy planAtomicOutput(std::size_t capacity) noexcept;
    static MediaRealtimeEdgePolicySet plan(
        const MediaGraphQueueParameters& queues) noexcept;

private:
    MediaBlockingEdgePolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph
