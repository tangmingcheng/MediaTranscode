#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <cstddef>
#include <deque>

namespace media::ffmpeg::graph {

class MediaGraphJitterBuffer final {
public:
    explicit MediaGraphJitterBuffer(MediaLatencyPolicy policy = {});

    void setPolicy(MediaLatencyPolicy policy) noexcept;
    const MediaLatencyPolicy& policy() const noexcept;

    void push(MediaBufferRef buffer);
    bool tryPop(MediaBufferRef& out);
    void clear();

    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    MediaLatencyPolicy m_policy;
    std::deque<MediaBufferRef> m_queue;
};

} // namespace media::ffmpeg::graph
