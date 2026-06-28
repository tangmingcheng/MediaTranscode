#include "internal/graph/runtime/queue/MediaQueueFactory.h"

#include "internal/graph/runtime/queue/MediaBlockingQueue.h"
#include "internal/graph/runtime/queue/MediaSpscQueue.h"
#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"

namespace media::ffmpeg::graph {

std::unique_ptr<MediaQueue> MediaQueueFactory::create(const MediaQueuePolicy& policy)
{
    switch (policy.mode) {
    case MediaQueueMode::SpscRing:
        return std::make_unique<MediaSpscRingQueue>(policy);

    case MediaQueueMode::Direct:
    case MediaQueueMode::Blocking:
    case MediaQueueMode::MpscRing:
    case MediaQueueMode::Unknown:
    default:
        return std::make_unique<MediaBlockingQueue>(policy);
    }
}

} // namespace media::ffmpeg::graph
