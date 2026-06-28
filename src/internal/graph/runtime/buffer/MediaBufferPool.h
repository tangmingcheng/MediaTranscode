#pragma once

#include "internal/graph/runtime/buffer/MediaBufferLease.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaBufferPoolOptions {
    std::size_t maxCachedBuffers = 32;
    bool dropNullBuffers = true;
};

struct MediaBufferPoolStats {
    uint64_t acquired = 0;
    uint64_t reused = 0;
    uint64_t created = 0;
    uint64_t released = 0;
    uint64_t dropped = 0;
    std::size_t cached = 0;
    std::size_t peakCached = 0;
};

class MediaBufferPool final {
public:
    using Factory = std::function<MediaBufferRef()>;

    explicit MediaBufferPool(Factory factory = {}, MediaBufferPoolOptions options = {});

    MediaBufferPool(const MediaBufferPool&) = delete;
    MediaBufferPool& operator=(const MediaBufferPool&) = delete;

    ::media::Result<MediaBufferLease> acquire();
    void release(MediaBufferRef buffer);
    void clear();
    void trim(std::size_t maxCachedBuffers);

    std::size_t cached() const;
    const MediaBufferPoolOptions& options() const noexcept;
    MediaBufferPoolStats stats() const;

private:
    void refreshCachedStatsLocked();

private:
    Factory m_factory;
    MediaBufferPoolOptions m_options;
    mutable std::mutex m_mutex;
    std::vector<MediaBufferRef> m_cached;
    MediaBufferPoolStats m_stats;
};

} // namespace media::ffmpeg::graph
