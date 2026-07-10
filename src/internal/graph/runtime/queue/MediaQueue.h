#pragma once

#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/queue/MediaQueueMetrics.h"
#include "media_transcode/Result.h"

#include <cstddef>

namespace media::ffmpeg::graph {

enum class MediaQueuePushOutcome {
    Accepted,
    Dropped,
    WouldBlock,
    Closed,
    Aborted
};

class MediaQueue {
public:
    virtual ~MediaQueue() = default;

    MediaQueue(const MediaQueue&) = delete;
    MediaQueue& operator=(const MediaQueue&) = delete;

    virtual ::media::Status push(MediaBufferRef buffer) = 0;
    virtual MediaQueuePushOutcome pushOutcome(MediaBufferRef buffer) = 0;
    virtual ::media::Status pop(MediaBufferRef& out) = 0;
    virtual bool tryPop(MediaBufferRef& out) = 0;

    virtual void close() = 0;
    virtual void abort() = 0;
    virtual void clear() = 0;

    virtual bool closed() const = 0;
    virtual bool aborted() const = 0;
    virtual std::size_t size() const = 0;
    virtual std::size_t capacity() const = 0;

    virtual const MediaQueuePolicy& policy() const noexcept = 0;
    virtual const MediaQueueMetrics& metrics() const noexcept = 0;

protected:
    MediaQueue() = default;
};

} // namespace media::ffmpeg::graph
