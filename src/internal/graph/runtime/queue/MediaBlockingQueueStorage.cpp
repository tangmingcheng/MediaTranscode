#include "internal/graph/runtime/queue/MediaBlockingQueueStorage.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

thread_local bool g_failNextPreparationAllocationForTesting = false;

} // namespace

std::size_t MediaBlockingQueueStorage::PreparedPush::size() const noexcept
{
    return m_buffers.size();
}

::media::Status MediaBlockingQueueStorage::PreparedPush::replace(
    std::span<const MediaBufferRef> buffers)
{
    if (buffers.size() != m_buffers.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared output replacement changed reserved capacity"));
    }
    for (const auto& buffer : buffers) {
        if (!buffer) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Prepared output replacement rejects null buffers"));
        }
    }
    for (std::size_t index = 0; index < buffers.size(); ++index) {
        m_buffers[index] = buffers[index];
    }
    return ::media::Status::success();
}

MediaBlockingQueueStorage::MediaBlockingQueueStorage(
    const MediaQueuePolicy& policy)
    : m_mode(policy.storageMode)
{
    if (m_mode == MediaQueueStorageMode::AtomicPrepared) {
        if (!policy.isBoundedQueue()) {
            throw std::invalid_argument(
                "Atomic prepared queue storage requires bounded capacity");
        }
        m_ring.resize(policy.capacity);
        return;
    }
    if (m_mode != MediaQueueStorageMode::Deque) {
        throw std::invalid_argument(
            "Blocking queue storage mode must be planner-configured");
    }
}

bool MediaBlockingQueueStorage::valid() const noexcept
{
    return m_mode == MediaQueueStorageMode::Deque ||
        m_mode == MediaQueueStorageMode::AtomicPrepared;
}

bool MediaBlockingQueueStorage::supportsPreparedPush() const noexcept
{
    return m_mode == MediaQueueStorageMode::AtomicPrepared;
}

bool MediaBlockingQueueStorage::empty() const noexcept
{
    return size() == 0;
}

std::size_t MediaBlockingQueueStorage::size() const noexcept
{
    return m_mode == MediaQueueStorageMode::Deque
        ? m_deque.size()
        : m_ringSize;
}

MediaBufferRef& MediaBlockingQueueStorage::front() noexcept
{
    return m_mode == MediaQueueStorageMode::Deque
        ? m_deque.front()
        : m_ring[m_ringHead];
}

const MediaBufferRef& MediaBlockingQueueStorage::at(
    std::size_t offset) const noexcept
{
    return m_mode == MediaQueueStorageMode::Deque
        ? m_deque[offset]
        : m_ring[ringIndex(offset)];
}

void MediaBlockingQueueStorage::pushBack(MediaBufferRef buffer)
{
    if (m_mode == MediaQueueStorageMode::Deque) {
        m_deque.push_back(std::move(buffer));
        return;
    }
    pushBackPrepared(std::move(buffer));
}

void MediaBlockingQueueStorage::popFront() noexcept
{
    if (m_mode == MediaQueueStorageMode::Deque) {
        m_deque.pop_front();
        return;
    }
    m_ring[m_ringHead].reset();
    m_ringHead = ringIndex(1);
    --m_ringSize;
    if (m_ringSize == 0) m_ringHead = 0;
}

void MediaBlockingQueueStorage::erase(std::size_t offset) noexcept
{
    if (m_mode == MediaQueueStorageMode::Deque) {
        m_deque.erase(m_deque.begin() +
                      static_cast<std::ptrdiff_t>(offset));
        return;
    }
    for (std::size_t index = offset; index + 1 < m_ringSize; ++index) {
        m_ring[ringIndex(index)] =
            std::move(m_ring[ringIndex(index + 1)]);
    }
    m_ring[ringIndex(m_ringSize - 1)].reset();
    --m_ringSize;
    if (m_ringSize == 0) m_ringHead = 0;
}

void MediaBlockingQueueStorage::clear() noexcept
{
    if (m_mode == MediaQueueStorageMode::Deque) {
        m_deque.clear();
        return;
    }
    for (std::size_t offset = 0; offset < m_ringSize; ++offset) {
        m_ring[ringIndex(offset)].reset();
    }
    m_ringHead = 0;
    m_ringSize = 0;
}

::media::Result<MediaBlockingQueueStorage::PreparedPush>
MediaBlockingQueueStorage::prepare(
    std::span<const MediaBufferRef> buffers) const
{
    if (!supportsPreparedPush()) {
        return ::media::Result<PreparedPush>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Atomic output requires planner-configured prepared storage"));
    }
    try {
        if (g_failNextPreparationAllocationForTesting) {
            g_failNextPreparationAllocationForTesting = false;
            throw std::bad_alloc();
        }
        PreparedPush prepared;
        prepared.m_buffers.reserve(buffers.size());
        for (const auto& buffer : buffers) {
            if (!buffer) {
                return ::media::Result<PreparedPush>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Prepared output rejects null buffers"));
            }
            prepared.m_buffers.push_back(buffer);
        }
        return ::media::Result<PreparedPush>::success(std::move(prepared));
    } catch (const std::bad_alloc&) {
        return ::media::Result<PreparedPush>::failure(
            ::media::ErrorInfo::internalError(
                "Prepared output storage allocation failed"));
    }
}

void MediaBlockingQueueStorage::publish(PreparedPush& prepared) noexcept
{
    for (auto& buffer : prepared.m_buffers) {
        pushBackPrepared(std::move(buffer));
    }
    prepared.m_buffers.clear();
}

void MediaBlockingQueueStorage::
    injectPreparationAllocationFailureForTesting() noexcept
{
    g_failNextPreparationAllocationForTesting = true;
}

std::size_t MediaBlockingQueueStorage::ringIndex(
    std::size_t offset) const noexcept
{
    return (m_ringHead + offset) % m_ring.size();
}

void MediaBlockingQueueStorage::pushBackPrepared(
    MediaBufferRef buffer) noexcept
{
    m_ring[ringIndex(m_ringSize)] = std::move(buffer);
    ++m_ringSize;
}

} // namespace media::ffmpeg::graph
