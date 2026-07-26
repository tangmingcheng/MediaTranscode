#pragma once

#include "internal/graph/runtime/queue/MediaQueue.h"

#include <condition_variable>
#include <list>
#include <mutex>
#include <span>

namespace media::ffmpeg::graph {

class MediaAtomicOutputTransaction;
class MediaReservedOutputTransaction;

class MediaBlockingQueue final : public MediaQueue {
public:
    class PreparedPush final {
    public:
        PreparedPush() = default;
        PreparedPush(PreparedPush&&) noexcept = default;
        PreparedPush& operator=(PreparedPush&&) noexcept = default;
        PreparedPush(const PreparedPush&) = delete;
        PreparedPush& operator=(const PreparedPush&) = delete;

    private:
        friend class MediaBlockingQueue;
        friend class MediaAtomicOutputTransaction;
        friend class MediaReservedOutputTransaction;
        std::list<MediaBufferRef> nodes;
    };

    explicit MediaBlockingQueue(MediaQueuePolicy policy = {});

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
    friend class MediaAtomicOutputTransaction;
    friend class MediaReservedOutputTransaction;

    ::media::Result<PreparedPush> preparePush(
        std::span<const MediaBufferRef> buffers) const;
    void publishPreparedLocked(PreparedPush& prepared) noexcept;
    void notifyPreparedPublished() noexcept;
    bool fullLocked() const;
    ::media::Status handleOverflowLocked(const MediaBufferRef& incoming);
    void updateSizeMetricsLocked();

private:
    MediaQueuePolicy m_policy;
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::list<MediaBufferRef> m_queue;
    bool m_closed = false;
    bool m_aborted = false;
    MediaQueueMetrics m_metrics;
};

} // namespace media::ffmpeg::graph
