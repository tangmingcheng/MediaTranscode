#include "internal/graph/runtime/buffer/MediaBufferPool.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaBufferPool::MediaBufferPool(MediaBufferPolicy policy)
    : m_policy(std::move(policy))
{
}

void MediaBufferPool::setAllocator(std::unique_ptr<MediaBufferAllocator> allocator)
{
    std::lock_guard lock(m_mutex);
    m_allocator = std::move(allocator);
    m_available.clear();
    m_metrics.pooledBuffers = 0;
}

void MediaBufferPool::setPolicy(MediaBufferPolicy policy)
{
    std::lock_guard lock(m_mutex);
    m_policy = std::move(policy);
}

::media::Result<MediaBufferRef> MediaBufferPool::acquire()
{
    std::lock_guard lock(m_mutex);

    if (!m_available.empty()) {
        MediaBufferRef buffer = std::move(m_available.back());
        m_available.pop_back();
        m_metrics.reusedBuffers++;
        m_metrics.pooledBuffers = m_available.size();
        return ::media::Result<MediaBufferRef>::success(std::move(buffer));
    }

    if (!m_allocator) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("MediaBufferPool acquire failed: allocator is null"));
    }

    auto result = m_allocator->allocate();
    if (!result) {
        return result;
    }

    m_metrics.allocatedBuffers++;
    return result;
}

void MediaBufferPool::release(MediaBufferRef buffer)
{
    if (!buffer) {
        return;
    }

    std::lock_guard lock(m_mutex);

    if (!m_policy.allowPoolReuse || !m_policy.usesPool()) {
        if (m_allocator) {
            m_allocator->release(std::move(buffer));
        }
        m_metrics.releasedBuffers++;
        return;
    }

    if (m_policy.memoryBudget.hasBufferLimit() &&
        m_available.size() >= m_policy.memoryBudget.maxBuffers) {
        m_metrics.droppedBuffers++;
        return;
    }

    m_available.push_back(std::move(buffer));
    m_metrics.releasedBuffers++;
    m_metrics.pooledBuffers = m_available.size();
}

void MediaBufferPool::clear()
{
    std::lock_guard lock(m_mutex);
    m_available.clear();
    m_metrics.pooledBuffers = 0;
}

std::size_t MediaBufferPool::size() const
{
    std::lock_guard lock(m_mutex);
    return m_available.size();
}

const MediaBufferPolicy& MediaBufferPool::policy() const noexcept
{
    return m_policy;
}

const MediaBufferMetrics& MediaBufferPool::metrics() const noexcept
{
    return m_metrics;
}

} // namespace media::ffmpeg::graph
