#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaPreparationStorageLease::MediaPreparationStorageLease(
    std::shared_ptr<MediaPreparationStorageBudget> owner,
    std::size_t bytes) noexcept
    : m_owner(std::move(owner)), m_bytes(bytes)
{
}

MediaPreparationStorageLease::~MediaPreparationStorageLease()
{
    release();
}

MediaPreparationStorageLease::MediaPreparationStorageLease(
    MediaPreparationStorageLease&& other) noexcept
    : m_owner(std::move(other.m_owner)), m_bytes(std::exchange(other.m_bytes, 0))
{
}

MediaPreparationStorageLease& MediaPreparationStorageLease::operator=(
    MediaPreparationStorageLease&& other) noexcept
{
    if (this != &other) {
        release();
        m_owner = std::move(other.m_owner);
        m_bytes = std::exchange(other.m_bytes, 0);
    }
    return *this;
}

void MediaPreparationStorageLease::release() noexcept
{
    if (m_owner) {
        m_owner->release(m_bytes);
        m_owner.reset();
    }
    m_bytes = 0;
}

MediaPreparationStorageBudget::MediaPreparationStorageBudget(
    std::size_t capacity) noexcept
    : m_storage(capacity)
{
}

::media::Result<std::shared_ptr<MediaPreparationStorageBudget>>
MediaPreparationStorageBudget::create(std::size_t capacity)
{
    using Result = ::media::Result<std::shared_ptr<MediaPreparationStorageBudget>>;
    if (capacity == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "preparation storage requires a positive planned capacity"));
    }
    try {
        return Result::success(std::shared_ptr<MediaPreparationStorageBudget>(
            new MediaPreparationStorageBudget(capacity)));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "preparation storage budget allocation failed"));
    }
}

::media::Result<MediaPreparationStorageLease>
MediaPreparationStorageBudget::reserve(std::size_t bytes)
{
    using Result = ::media::Result<MediaPreparationStorageLease>;
    std::scoped_lock lock(m_mutex);
    if (bytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "preparation storage reservation must be positive"));
    }
    if (!m_storage.retain(bytes)) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "preparation storage reservation exceeds the admitted capacity"));
    }
    m_highWaterBytes = (std::max)(m_highWaterBytes, m_storage.retainedBytes());
    return Result::success(MediaPreparationStorageLease(shared_from_this(), bytes));
}

void MediaPreparationStorageBudget::release(std::size_t bytes) noexcept
{
    std::scoped_lock lock(m_mutex);
    const bool released = m_storage.release(bytes);
    // Only leases created after a successful retain can enter this method.
    assert(released);
    (void)released;
}

MediaPreparationStorageSnapshot MediaPreparationStorageBudget::snapshot() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return {m_storage.capacity(), m_storage.retainedBytes(), m_highWaterBytes};
}

} // namespace media::ffmpeg::graph
