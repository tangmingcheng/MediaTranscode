#pragma once

#include "internal/graph/runtime/queue/MediaBlockingQueue.h"

namespace media::ffmpeg::graph {

class MediaSpscQueue final : public MediaQueue {
public:
    explicit MediaSpscQueue(MediaQueuePolicy policy = {});

    ::media::Status push(MediaBufferRef buffer) override;
    MediaQueuePushOutcome pushOutcome(MediaBufferRef buffer) override;
    ::media::Status pop(MediaBufferRef& out) override;
    bool tryPop(MediaBufferRef& out) override;

    void close() override;
    void abort() override;
    void clear() override;

    bool closed() const override;
    bool aborted() const override;
    std::size_t size() const override;
    std::size_t capacity() const override;

    const MediaQueuePolicy& policy() const noexcept override;
    const MediaQueueMetrics& metrics() const noexcept override;

private:
    MediaBlockingQueue m_queue;
};

} // namespace media::ffmpeg::graph
