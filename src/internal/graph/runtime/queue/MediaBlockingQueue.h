#pragma once

#include "internal/graph/runtime/queue/MediaQueue.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaBlockingQueue final : public MediaQueue {
public:
    explicit MediaBlockingQueue(MediaQueuePolicy policy = {});

    ::media::Status push(MediaBufferRef buffer) override;
    bool tryPush(MediaBufferRef buffer) override;
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
    bool fullLocked() const;
    ::media::Status handleOverflowLocked(const MediaBufferRef& incoming);
    void updateSizeMetricsLocked();

private:
    MediaQueuePolicy m_policy;
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::deque<MediaBufferRef> m_queue;
    bool m_closed = false;
    bool m_aborted = false;
    MediaQueueMetrics m_metrics;
};

} // namespace media::ffmpeg::graph
