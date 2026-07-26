#include "internal/graph/runtime/queue/MediaBlockingQueue.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <new>

namespace media::ffmpeg::graph {
namespace {

bool overflowPolicyDropsIncoming(const MediaQueuePolicy& policy, const MediaBufferRef& buffer) noexcept
{
    if (policy.overflowPolicy == MediaQueueOverflowPolicy::DropNewest) {
        return true;
    }
    return policy.overflowPolicy == MediaQueueOverflowPolicy::DropNonKeyFrame &&
           buffer &&
           !buffer->isKeyFrame();
}

} // namespace

::media::Result<MediaBlockingQueue::PreparedPush>
MediaBlockingQueue::preparePush(
    std::span<const MediaBufferRef> buffers) const
{
    PreparedPush prepared;
    try {
        for (const auto& buffer : buffers) {
            if (!buffer) {
                return ::media::Result<PreparedPush>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaBlockingQueue preparation rejects null buffers"));
            }
            prepared.nodes.push_back(buffer);
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<PreparedPush>::failure(
            ::media::ErrorInfo::internalError(
                "MediaBlockingQueue could not allocate prepared output slots"));
    }
    return ::media::Result<PreparedPush>::success(std::move(prepared));
}

void MediaBlockingQueue::publishPreparedLocked(
    PreparedPush& prepared) noexcept
{
    const std::size_t count = prepared.nodes.size();
    m_queue.splice(m_queue.end(), prepared.nodes);
    m_metrics.pushed.fetch_add(count, std::memory_order_relaxed);
    updateSizeMetricsLocked();
}

void MediaBlockingQueue::notifyPreparedPublished() noexcept
{
    m_notEmpty.notify_all();
}

MediaBlockingQueue::MediaBlockingQueue(MediaQueuePolicy policy)
    : m_policy(std::move(policy))
{
    if (m_policy.mode == MediaQueueMode::Unknown) {
        m_policy.mode = MediaQueueMode::Blocking;
    }
}

::media::Status MediaBlockingQueue::push(MediaBufferRef buffer)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaBlockingQueue push failed: buffer is null"));
    }

    std::unique_lock lock(m_mutex);

    if (m_aborted) {
        m_metrics.failedPushes++;
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("MediaBlockingQueue push failed: queue aborted"));
    }

    if (m_closed) {
        m_metrics.failedPushes++;
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled("MediaBlockingQueue push interrupted: queue closed"));
    }

    while (fullLocked()) {
        if (m_policy.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer) {
            auto status = handleOverflowLocked(buffer);
            if (!status || !fullLocked()) {
                break;
            }
            if (overflowPolicyDropsIncoming(m_policy, buffer)) {
                return ::media::Status::success();
            }
            if (m_policy.overflowPolicy == MediaQueueOverflowPolicy::DropNonKeyFrame &&
                buffer->isKeyFrame()) {
                m_metrics.blockedPushes++;
                m_notFull.wait(lock, [&] { return !fullLocked() || m_closed || m_aborted; });
                continue;
            }
        } else {
            m_metrics.blockedPushes++;
            m_notFull.wait(lock, [&] { return !fullLocked() || m_closed || m_aborted; });
        }

        if (m_aborted) {
            m_metrics.failedPushes++;
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("MediaBlockingQueue push failed: queue aborted"));
        }
        if (m_closed) {
            m_metrics.failedPushes++;
            return ::media::Status::failure(
                ::media::ErrorInfo::cancelled("MediaBlockingQueue push interrupted: queue closed"));
        }
    }

    if (fullLocked()) {
        m_metrics.failedPushes++;
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("MediaBlockingQueue push failed: queue full"));
    }

    m_queue.push_back(std::move(buffer));
    m_metrics.pushed++;
    updateSizeMetricsLocked();
    lock.unlock();
    m_notEmpty.notify_one();
    return ::media::Status::success();
}

MediaQueuePushOutcome MediaBlockingQueue::pushOutcome(MediaBufferRef buffer)
{
    if (!buffer) {
        return MediaQueuePushOutcome::WouldBlock;
    }

    std::lock_guard lock(m_mutex);
    if (m_aborted) {
        m_metrics.failedPushes++;
        return MediaQueuePushOutcome::Aborted;
    }
    if (m_closed) {
        m_metrics.failedPushes++;
        return MediaQueuePushOutcome::Closed;
    }

    if (fullLocked()) {
        switch (m_policy.overflowPolicy) {
        case MediaQueueOverflowPolicy::DropNewest:
            m_metrics.dropped++;
            return MediaQueuePushOutcome::Dropped;
        case MediaQueueOverflowPolicy::DropNonKeyFrame:
            if (!buffer->isKeyFrame()) {
                m_metrics.dropped++;
                return MediaQueuePushOutcome::Dropped;
            }
            for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
                if (*it && !(*it)->isKeyFrame()) {
                    m_queue.erase(it);
                    m_metrics.dropped++;
                    updateSizeMetricsLocked();
                    break;
                }
            }
            if (fullLocked()) {
                return MediaQueuePushOutcome::WouldBlock;
            }
            break;
        case MediaQueueOverflowPolicy::DropOldest:
            m_queue.pop_front();
            m_metrics.dropped++;
            updateSizeMetricsLocked();
            break;
        case MediaQueueOverflowPolicy::Abort:
            m_aborted = true;
            m_closed = true;
            m_metrics.failedPushes++;
            m_notEmpty.notify_all();
            m_notFull.notify_all();
            return MediaQueuePushOutcome::Aborted;
        case MediaQueueOverflowPolicy::BlockProducer:
        default:
            return MediaQueuePushOutcome::WouldBlock;
        }
    }

    m_queue.push_back(std::move(buffer));
    m_metrics.pushed++;
    updateSizeMetricsLocked();
    m_notEmpty.notify_one();
    return MediaQueuePushOutcome::Accepted;
}

::media::Status MediaBlockingQueue::pop(MediaBufferRef& out)
{
    std::unique_lock lock(m_mutex);
    m_notEmpty.wait(lock, [&] { return !m_queue.empty() || m_closed || m_aborted; });

    if (m_aborted) {
        m_metrics.failedPops++;
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("MediaBlockingQueue pop failed: queue aborted"));
    }

    if (m_queue.empty()) {
        m_metrics.failedPops++;
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled("MediaBlockingQueue pop interrupted: queue closed and empty"));
    }

    out = std::move(m_queue.front());
    m_queue.pop_front();
    m_metrics.popped++;
    updateSizeMetricsLocked();
    lock.unlock();
    m_notFull.notify_one();
    return ::media::Status::success();
}

bool MediaBlockingQueue::tryPop(MediaBufferRef& out)
{
    std::lock_guard lock(m_mutex);
    if (m_queue.empty()) {
        return false;
    }

    out = std::move(m_queue.front());
    m_queue.pop_front();
    m_metrics.popped++;
    updateSizeMetricsLocked();
    m_notFull.notify_one();
    return true;
}

void MediaBlockingQueue::close()
{
    std::lock_guard lock(m_mutex);
    m_closed = true;
    m_notEmpty.notify_all();
    m_notFull.notify_all();
}

void MediaBlockingQueue::abort()
{
    std::lock_guard lock(m_mutex);
    m_aborted = true;
    m_closed = true;
    m_notEmpty.notify_all();
    m_notFull.notify_all();
}

void MediaBlockingQueue::clear()
{
    std::lock_guard lock(m_mutex);
    m_queue.clear();
    updateSizeMetricsLocked();
    m_notFull.notify_all();
}

bool MediaBlockingQueue::closed() const
{
    std::lock_guard lock(m_mutex);
    return m_closed;
}

bool MediaBlockingQueue::aborted() const
{
    std::lock_guard lock(m_mutex);
    return m_aborted;
}

std::size_t MediaBlockingQueue::size() const
{
    std::lock_guard lock(m_mutex);
    return m_queue.size();
}

std::size_t MediaBlockingQueue::capacity() const
{
    return m_policy.capacity;
}

const MediaQueuePolicy& MediaBlockingQueue::policy() const noexcept
{
    return m_policy;
}

const MediaQueueMetrics& MediaBlockingQueue::metrics() const noexcept
{
    return m_metrics;
}

bool MediaBlockingQueue::fullLocked() const
{
    return m_policy.isBoundedQueue() && m_queue.size() >= m_policy.capacity;
}

::media::Status MediaBlockingQueue::handleOverflowLocked(const MediaBufferRef& incoming)
{
    switch (m_policy.overflowPolicy) {
    case MediaQueueOverflowPolicy::DropNewest:
        m_metrics.dropped++;
        return ::media::Status::success();

    case MediaQueueOverflowPolicy::DropOldest:
        if (!m_queue.empty()) {
            m_queue.pop_front();
            m_metrics.dropped++;
            updateSizeMetricsLocked();
        }
        return ::media::Status::success();

    case MediaQueueOverflowPolicy::DropNonKeyFrame:
        if (incoming && !incoming->isKeyFrame()) {
            m_metrics.dropped++;
            return ::media::Status::success();
        }
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (*it && !(*it)->isKeyFrame()) {
                m_queue.erase(it);
                m_metrics.dropped++;
                updateSizeMetricsLocked();
                return ::media::Status::success();
            }
        }
        return ::media::Status::success();

    case MediaQueueOverflowPolicy::Abort:
        m_aborted = true;
        m_closed = true;
        m_metrics.failedPushes++;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("MediaBlockingQueue aborted by overflow policy"));

    case MediaQueueOverflowPolicy::BlockProducer:
    default:
        return ::media::Status::success();
    }
}

void MediaBlockingQueue::updateSizeMetricsLocked()
{
    m_metrics.currentSize = m_queue.size();
    const std::size_t current = m_metrics.currentSize.load();
    if (current > m_metrics.peakSize.load()) {
        m_metrics.peakSize = current;
    }
}

} // namespace media::ffmpeg::graph
