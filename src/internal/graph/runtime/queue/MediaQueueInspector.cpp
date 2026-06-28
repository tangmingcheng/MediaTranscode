#include "internal/graph/runtime/queue/MediaQueueInspector.h"

namespace media::ffmpeg::graph {

MediaQueueSnapshot MediaQueueInspector::inspect(const MediaQueue& queue)
{
    MediaQueueSnapshot snapshot;
    snapshot.policy = queue.policy();
    snapshot.metrics = queue.metrics();
    snapshot.size = queue.size();
    snapshot.capacity = queue.capacity();
    snapshot.closed = queue.closed();
    snapshot.aborted = queue.aborted();
    snapshot.saturated = snapshot.capacity > 0 && snapshot.size >= snapshot.capacity;
    snapshot.healthy = !snapshot.aborted && !snapshot.saturated;

    snapshot.summary = "queue size=" + std::to_string(snapshot.size) +
                       ", capacity=" + std::to_string(snapshot.capacity) +
                       ", pushed=" + std::to_string(snapshot.metrics.pushed) +
                       ", popped=" + std::to_string(snapshot.metrics.popped) +
                       ", dropped=" + std::to_string(snapshot.metrics.dropped);
    return snapshot;
}

} // namespace media::ffmpeg::graph
