#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

class ScopedWaiter final {
public:
    explicit ScopedWaiter(std::atomic_size_t& count) noexcept : m_count(count) { ++m_count; }
    ~ScopedWaiter() { --m_count; }
    ScopedWaiter(const ScopedWaiter&) = delete;
    ScopedWaiter& operator=(const ScopedWaiter&) = delete;
private:
    std::atomic_size_t& m_count;
};

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

MediaSpscRingQueue::MediaSpscRingQueue(MediaQueuePolicy policy)
    : m_policy(std::move(policy))
{
    if (m_policy.mode == MediaQueueMode::Unknown || m_policy.mode == MediaQueueMode::Blocking) {
        m_policy.mode = MediaQueueMode::SpscRing;
    }

    if (m_policy.capacity == 0) {
        m_policy.capacity = 16;
    }

    m_capacity = m_policy.capacity + 1;
    m_ring.resize(m_capacity);
}

::media::Status MediaSpscRingQueue::push(MediaBufferRef buffer)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaSpscRingQueue push failed: buffer is null"));
    }

    for (;;) {
        const MediaQueuePushOutcome outcome = pushOutcome(buffer);
        if (outcome == MediaQueuePushOutcome::Accepted ||
            outcome == MediaQueuePushOutcome::Dropped) {
            return ::media::Status::success();
        }
        if (outcome == MediaQueuePushOutcome::Aborted) {
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("MediaSpscRingQueue push failed: queue aborted"));
        }
        if (outcome == MediaQueuePushOutcome::Closed) {
            return ::media::Status::failure(
                ::media::ErrorInfo::cancelled("MediaSpscRingQueue push interrupted: queue closed"));
        }

        if (m_policy.overflowPolicy == MediaQueueOverflowPolicy::DropNonKeyFrame &&
            buffer->isKeyFrame()) {
            ++m_metrics.blockedPushes;
            std::unique_lock<std::mutex> lock(m_mutex);
            const ScopedWaiter waiter(m_metrics.blockedProducers);
            m_notFull.wait(lock, [&] {
                const auto write = m_write.load(std::memory_order_acquire);
                const auto read = m_read.load(std::memory_order_acquire);
                return m_closed || m_aborted || !full(write, read);
            });
            continue;
        }

        if (m_policy.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer) {
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("MediaSpscRingQueue push failed: overflow policy could not accept buffer"));
        }

        ++m_metrics.blockedPushes;
        std::unique_lock<std::mutex> lock(m_mutex);
        const ScopedWaiter waiter(m_metrics.blockedProducers);
        m_notFull.wait(lock, [&] {
            const auto write = m_write.load(std::memory_order_acquire);
            const auto read = m_read.load(std::memory_order_acquire);
            return m_closed || m_aborted || !full(write, read);
        });
    }
}

MediaQueuePushOutcome MediaSpscRingQueue::pushOutcome(MediaBufferRef buffer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!buffer) {
        ++m_metrics.failedPushes;
        return MediaQueuePushOutcome::WouldBlock;
    }
    if (m_aborted) {
        ++m_metrics.failedPushes;
        return MediaQueuePushOutcome::Aborted;
    }
    if (m_closed) {
        ++m_metrics.failedPushes;
        return MediaQueuePushOutcome::Closed;
    }

    const std::size_t write = m_write.load(std::memory_order_relaxed);
    const std::size_t read = m_read.load(std::memory_order_acquire);

    if (full(write, read)) {
        switch (m_policy.overflowPolicy) {
        case MediaQueueOverflowPolicy::DropOldest:
            if (!dropOldest()) {
                ++m_metrics.failedPushes;
                return MediaQueuePushOutcome::WouldBlock;
            }
            break;
        case MediaQueueOverflowPolicy::DropNewest:
            ++m_metrics.dropped;
            return MediaQueuePushOutcome::Dropped;
        case MediaQueueOverflowPolicy::Abort:
            m_aborted = true;
            m_closed = true;
            ++m_metrics.failedPushes;
            m_notEmpty.notify_all();
            m_notFull.notify_all();
            return MediaQueuePushOutcome::Aborted;
        case MediaQueueOverflowPolicy::DropNonKeyFrame:
            if (!buffer->isKeyFrame()) {
                ++m_metrics.dropped;
                return MediaQueuePushOutcome::Dropped;
            }
            if (!dropOldestNonKeyFrame()) {
                return MediaQueuePushOutcome::WouldBlock;
            }
            break;
        case MediaQueueOverflowPolicy::BlockProducer:
        default:
            ++m_metrics.failedPushes;
            return MediaQueuePushOutcome::WouldBlock;
        }
    }

    const std::size_t currentWrite = m_write.load(std::memory_order_relaxed);
    m_ring[currentWrite] = std::move(buffer);
    m_write.store(next(currentWrite), std::memory_order_release);
    ++m_metrics.pushed;
    updateSizeMetrics(sizeLocked());
    m_notEmpty.notify_one();
    return MediaQueuePushOutcome::Accepted;
}

::media::Status MediaSpscRingQueue::pop(MediaBufferRef& out)
{
    while (!tryPop(out)) {
        if (m_aborted) {
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("MediaSpscRingQueue pop failed: queue aborted"));
        }

        if (m_closed) {
            return ::media::Status::failure(
                ::media::ErrorInfo::cancelled("MediaSpscRingQueue pop interrupted: queue closed and empty"));
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        const ScopedWaiter waiter(m_metrics.blockedConsumers);
        m_notEmpty.wait(lock, [&] {
            const auto write = m_write.load(std::memory_order_acquire);
            const auto read = m_read.load(std::memory_order_acquire);
            return m_closed || m_aborted || !empty(write, read);
        });
    }

    return ::media::Status::success();
}

bool MediaSpscRingQueue::tryPop(MediaBufferRef& out)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::size_t read = m_read.load(std::memory_order_relaxed);
    const std::size_t write = m_write.load(std::memory_order_acquire);

    if (empty(write, read)) {
        ++m_metrics.failedPops;
        return false;
    }

    out = std::move(m_ring[read]);
    m_ring[read].reset();
    m_read.store(next(read), std::memory_order_release);
    ++m_metrics.popped;
    updateSizeMetrics(sizeLocked());
    m_notFull.notify_one();
    return true;
}

void MediaSpscRingQueue::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    m_notEmpty.notify_all();
    m_notFull.notify_all();
}

void MediaSpscRingQueue::abort()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aborted = true;
    m_closed = true;
    m_notEmpty.notify_all();
    m_notFull.notify_all();
}

void MediaSpscRingQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& item : m_ring) {
        item.reset();
    }
    m_read.store(0, std::memory_order_release);
    m_write.store(0, std::memory_order_release);
    updateSizeMetrics(0);
    m_notFull.notify_all();
}

bool MediaSpscRingQueue::closed() const
{
    return m_closed;
}

bool MediaSpscRingQueue::aborted() const
{
    return m_aborted;
}

std::size_t MediaSpscRingQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return sizeLocked();
}

std::size_t MediaSpscRingQueue::capacity() const
{
    return m_policy.capacity;
}

const MediaQueuePolicy& MediaSpscRingQueue::policy() const noexcept
{
    return m_policy;
}

const MediaQueueMetrics& MediaSpscRingQueue::metrics() const noexcept
{
    return m_metrics;
}

bool MediaSpscRingQueue::full(std::size_t write, std::size_t read) const noexcept
{
    return next(write) == read;
}

bool MediaSpscRingQueue::empty(std::size_t write, std::size_t read) const noexcept
{
    return write == read;
}

std::size_t MediaSpscRingQueue::next(std::size_t index) const noexcept
{
    return (index + 1) % m_capacity;
}

bool MediaSpscRingQueue::dropOldest()
{
    const std::size_t read = m_read.load(std::memory_order_relaxed);
    const std::size_t write = m_write.load(std::memory_order_acquire);
    if (empty(write, read)) {
        return false;
    }

    m_ring[read].reset();
    m_read.store(next(read), std::memory_order_release);
    ++m_metrics.dropped;
    updateSizeMetrics(sizeLocked());
    return true;
}

bool MediaSpscRingQueue::dropOldestNonKeyFrame()
{
    const std::size_t read = m_read.load(std::memory_order_relaxed);
    const std::size_t write = m_write.load(std::memory_order_acquire);
    if (empty(write, read)) {
        return false;
    }

    std::size_t candidate = read;
    while (candidate != write) {
        if (m_ring[candidate] && !m_ring[candidate]->isKeyFrame()) {
            std::size_t current = candidate;
            while (current != read) {
                const std::size_t previous = current == 0 ? m_capacity - 1 : current - 1;
                m_ring[current] = std::move(m_ring[previous]);
                current = previous;
            }
            m_ring[read].reset();
            m_read.store(next(read), std::memory_order_release);
            ++m_metrics.dropped;
            updateSizeMetrics(sizeLocked());
            return true;
        }
        candidate = next(candidate);
    }

    return false;
}

std::size_t MediaSpscRingQueue::sizeLocked() const noexcept
{
    const std::size_t write = m_write.load(std::memory_order_acquire);
    const std::size_t read = m_read.load(std::memory_order_acquire);
    if (write >= read) {
        return write - read;
    }
    return m_capacity - read + write;
}

void MediaSpscRingQueue::updateSizeMetrics(std::size_t current) noexcept
{
    m_metrics.currentSize = current;
    if (current > m_metrics.peakSize) {
        m_metrics.peakSize = current;
    }
}

} // namespace media::ffmpeg::graph
