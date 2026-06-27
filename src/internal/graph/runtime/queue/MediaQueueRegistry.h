#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/queue/MediaQueue.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaQueueRegistry final {
public:
    ::media::Status registerQueue(MediaEdgeId edgeId, std::unique_ptr<MediaQueue> queue);

    MediaQueue* find(MediaEdgeId edgeId);
    const MediaQueue* find(MediaEdgeId edgeId) const;

    bool remove(MediaEdgeId edgeId);
    void clear();
    std::size_t size() const noexcept;

private:
    std::unordered_map<uint32_t, std::unique_ptr<MediaQueue>> m_queues;
};

} // namespace media::ffmpeg::graph
