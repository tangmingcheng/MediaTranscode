#include "internal/graph/runtime/buffer/MediaBufferPool.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaBufferPool::MediaBufferPool(Factory factory, MediaBufferPoolOptions options)
    : m_factory(std::move(factory))
    , m_options(options)
{
}

::media::Result<MediaBufferLease> MediaBufferPool::acquire()
{
    MediaBufferRef buffer;

    {
        std::lock_guard lock(m_mutex);
        ++m_stats.acquired;
        if (!m_cached.empty()) {
            buffer = std::move(m_cached.back());
            m_cached.pop_back();
            ++m_stats.reused;
            refreshCachedStatsLocked();
        }
    }

    if (!buffer) {
        if (!m_factory) {
            return ::media::Result<MediaBufferLease>::failure(
                ::media::ErrorInfo::notInitialized("MediaBufferPool acquire failed: factory is not set"));
        }

        buffer = m_factory();
        if (!buffer) {
            return ::media::Result<MediaBufferLease>::failure(
                ::media::ErrorInfo::invalidArgument("MediaBufferPool acquire failed: factory returned null buffer"));
        }

        std::lock_guard lock(m_mutex);
        ++m_stats.created;
    }

    MediaBufferLease lease(
        std::move(buffer),
        [this](MediaBufferRef released) {
            release(std::move(released));
        });
    return ::media::Result<MediaBufferLease>::success(std::move(lease));
}

void MediaBufferPool::release(MediaBufferRef buffer)
{
    std::lock_guard lock(m_mutex);
    if (!buffer && m_options.dropNullBuffers) {
        ++m_stats.dropped;
        return;
    }

    ++m_stats.released;
    if (m_cached.size() >= m_options.maxCachedBuffers) {
        ++m_stats.dropped;
        return;
    }

    m_cached.push_back(std::move(buffer));
    refreshCachedStatsLocked();
}

void MediaBufferPool::clear()
{
    std::lock_guard lock(m_mutex);
    m_cached.clear();
    refreshCachedStatsLocked();
}

void MediaBufferPool::trim(std::size_t maxCachedBuffers)
{
    std::lock_guard lock(m_mutex);
    while (m_cached.size() > maxCachedBuffers) {
        m_cached.pop_back();
        ++m_stats.dropped;
    }
    refreshCachedStatsLocked();
}

std::size_t MediaBufferPool::cached() const
{
    std::lock_guard lock(m_mutex);
    return m_cached.size();
}

const MediaBufferPoolOptions& MediaBufferPool::options() const noexcept
{
    return m_options;
}

MediaBufferPoolStats MediaBufferPool::stats() const
{
    std::lock_guard lock(m_mutex);
    return m_stats;
}

void MediaBufferPool::refreshCachedStatsLocked()
{
    m_stats.cached = m_cached.size();
    if (m_stats.cached > m_stats.peakCached) {
        m_stats.peakCached = m_stats.cached;
    }
}

} // namespace media::ffmpeg::graph
