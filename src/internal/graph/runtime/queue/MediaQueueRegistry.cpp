#include "internal/graph/runtime/queue/MediaQueueRegistry.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaQueueRegistry::registerQueue(MediaEdgeId edgeId, std::unique_ptr<MediaQueue> queue)
{
    if (!edgeId) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaQueueRegistry registerQueue failed: edge id is invalid"));
    }

    if (!queue) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaQueueRegistry registerQueue failed: queue is null"));
    }

    m_queues[edgeId.value] = std::move(queue);
    return ::media::Status::success();
}

MediaQueue* MediaQueueRegistry::find(MediaEdgeId edgeId)
{
    const auto it = m_queues.find(edgeId.value);
    return it == m_queues.end() ? nullptr : it->second.get();
}

const MediaQueue* MediaQueueRegistry::find(MediaEdgeId edgeId) const
{
    const auto it = m_queues.find(edgeId.value);
    return it == m_queues.end() ? nullptr : it->second.get();
}

bool MediaQueueRegistry::remove(MediaEdgeId edgeId)
{
    return m_queues.erase(edgeId.value) > 0;
}

void MediaQueueRegistry::clear()
{
    m_queues.clear();
}

std::size_t MediaQueueRegistry::size() const noexcept
{
    return m_queues.size();
}

} // namespace media::ffmpeg::graph
