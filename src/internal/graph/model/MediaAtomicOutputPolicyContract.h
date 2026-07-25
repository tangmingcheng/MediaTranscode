#pragma once

#include "internal/graph/core/MediaEdge.h"

namespace media::ffmpeg::graph {

class MediaAtomicOutputPolicyContract final {
public:
    static constexpr bool accepts(const MediaEdgePolicy& policy) noexcept
    {
        const auto& queue = policy.queuePolicy;
        return queue.bounded && queue.capacity > 0 &&
            queue.overflowPolicy == MediaQueueOverflowPolicy::BlockProducer &&
            queue.orderingPolicy == MediaQueueOrderingPolicy::Fifo &&
            queue.preserveOrdering;
    }

private:
    MediaAtomicOutputPolicyContract() = delete;
};

} // namespace media::ffmpeg::graph
