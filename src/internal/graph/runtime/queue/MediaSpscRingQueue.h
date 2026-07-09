#pragma once

#include "internal/graph/runtime/queue/MediaQueue.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class MediaSpscRingQueue final : public MediaQueue {
public:
    explicit MediaSpscRingQueue(MediaQueuePolicy policy = {});
    ~MediaSpscRingQueue() override = default;

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
    bool full(std::size_t write, std::size_t read) const noexcept;
    bool empty(std::size_t write, std::size_t read) const noexcept;
    std::size_t next(std::size_t index) const noexcept;
    bool dropOldest();
    std::size_t sizeLocked() const noexcept;
    void updateSizeMetrics(std::size_t current) noexcept;

private:
    MediaQueuePolicy m_policy;
    std::vector<MediaBufferRef> m_ring;
    std::size_t m_capacity = 0;
    mutable std::mutex m_mutex;
    std::atomic_size_t m_read{ 0 };
    std::atomic_size_t m_write{ 0 };
    std::atomic_bool m_closed{ false };
    std::atomic_bool m_aborted{ false };
    MediaQueueMetrics m_metrics;
};

} // namespace media::ffmpeg::graph
