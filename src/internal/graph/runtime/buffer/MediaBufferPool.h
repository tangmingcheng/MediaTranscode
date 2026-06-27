#pragma once

#include "internal/graph/model/MediaBufferPolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferAllocator.h"
#include "internal/graph/runtime/buffer/MediaBufferMetrics.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class MediaBufferPool final {
public:
    MediaBufferPool() = default;
    explicit MediaBufferPool(MediaBufferPolicy policy);

    MediaBufferPool(const MediaBufferPool&) = delete;
    MediaBufferPool& operator=(const MediaBufferPool&) = delete;

    void setAllocator(std::unique_ptr<MediaBufferAllocator> allocator);
    void setPolicy(MediaBufferPolicy policy);

    ::media::Result<MediaBufferRef> acquire();
    void release(MediaBufferRef buffer);
    void clear();

    std::size_t size() const;
    const MediaBufferPolicy& policy() const noexcept;
    const MediaBufferMetrics& metrics() const noexcept;

private:
    MediaBufferPolicy m_policy;
    std::unique_ptr<MediaBufferAllocator> m_allocator;
    mutable std::mutex m_mutex;
    std::vector<MediaBufferRef> m_available;
    MediaBufferMetrics m_metrics;
};

} // namespace media::ffmpeg::graph
