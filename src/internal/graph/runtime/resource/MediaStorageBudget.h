#pragma once

#include "internal/graph/runtime/resource/MediaByteReservationCounter.h"
#include "internal/graph/runtime/resource/MediaStorageReleaseNotification.h"
#include "internal/graph/runtime/resource/MediaBudgetBootstrapAllocator.h"
#include "media_transcode/Result.h"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <variant>

namespace media::ffmpeg::graph {

template <typename Domain> class MediaStorageBudget;

template <typename Domain>
struct MediaStandaloneStorage final {};
template <typename Domain>
struct MediaSharedStorage final {
    std::shared_ptr<MediaStorageBudget<Domain>> root;
};
template <typename Domain>
using MediaStorageScope = std::variant<
    MediaStandaloneStorage<Domain>, MediaSharedStorage<Domain>>;

template <typename Domain>
class MediaStorageLease final {
public:
    ~MediaStorageLease();
    MediaStorageLease(const MediaStorageLease<Domain>&) = delete;
    MediaStorageLease<Domain>& operator=(const MediaStorageLease<Domain>&) = delete;
    MediaStorageLease(MediaStorageLease<Domain>&& other) noexcept;
    MediaStorageLease<Domain>& operator=(MediaStorageLease<Domain>&& other) noexcept;
    std::size_t bytes() const noexcept { return m_bytes; }
    explicit operator bool() const noexcept { return m_owner != nullptr; }
    // Requires exclusive access before publishing the lease. Only unused
    // reservation may be returned; the retained allocation must remain covered.
    ::media::Status shrinkTo(std::size_t bytes);
    // Exclusive, unpublished ownership only. Splitting transfers credit without
    // changing the ledger or notifying waiters; taking all leaves this empty.
    ::media::Result<MediaStorageLease<Domain>> split(std::size_t bytes);
    bool belongsTo(const MediaStorageBudget<Domain>& budget) const noexcept
    { return m_owner.get() == &budget; }

private:
    friend class MediaStorageBudget<Domain>;
    MediaStorageLease(
        std::shared_ptr<MediaStorageBudget<Domain>> owner,
        std::size_t bytes) noexcept;
    void release() noexcept;
    std::shared_ptr<MediaStorageBudget<Domain>> m_owner;
    std::size_t m_bytes = 0;
};

template <typename Domain>
struct MediaStorageSnapshot final {
    std::size_t capacity;
    // Charged credit includes backing guarantees, even while locally unused.
    // It is not a count of currently live allocator bytes, RSS, or VRAM.
    std::size_t retainedBytes;
    std::size_t highWaterBytes;
};

template <typename Domain>
struct MediaStoragePartitionSnapshot final {
    std::shared_ptr<const MediaStorageBudget<Domain>> root;
    MediaStorageSnapshot<Domain> budget;
    std::size_t selectedRetainedBytes;
    std::size_t otherRetainedBytes;
};

// Domain separates independent admitted capacities at compile time.
// Consumers must honor the typed size plan and retain leases with storage.
template <typename Domain>
class MediaStorageBudget final
    : public std::enable_shared_from_this<MediaStorageBudget<Domain>> {
    class ConstructionKey final {
        friend class MediaStorageBudget;
        ConstructionKey() = default;
    };
public:
    using BootstrapAllocator = MediaBudgetBootstrapAllocator<MediaStorageBudget<Domain>,
        MediaStorageBudget<Domain>, MediaStorageLease<Domain>>;
    // Public only for allocator_traits; the private key restricts creation.
    MediaStorageBudget(ConstructionKey, std::size_t capacity,
        std::shared_ptr<const MediaStorageReleaseNotification> notification) noexcept
        : MediaStorageBudget(capacity, nullptr, std::move(notification)) {}
    static ::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>> create(
        std::size_t capacity, const MediaStorageScope<Domain>& scope,
        const MediaStorageReleaseObservation& observation);
    // Transfer an already admitted lease. Child allocations consume this credit
    // locally; the parent is charged once until the last child lease is gone.
    static ::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>> fromReservation(
        MediaStorageLease<Domain> reservation,
        const MediaStorageReleaseObservation& observation);
    ::media::Result<MediaStorageLease<Domain>> reserve(std::size_t bytes, MediaStorageAdmission admission);
    MediaStorageSnapshot<Domain> snapshot() const noexcept;
    // One ledger observation, not a reservation of future headroom. Caller
    // supplies storage; no allocator or callback runs under the ledger lock.
    static ::media::Result<MediaStoragePartitionSnapshot<Domain>> snapshotPartition(
        std::span<const std::shared_ptr<MediaStorageBudget<Domain>>> children,
        std::span<MediaStorageSnapshot<Domain>> snapshots);

private:
    friend class MediaStorageLease<Domain>;
    MediaStorageBudget(std::size_t capacity,
        std::shared_ptr<MediaStorageBudget<Domain>> parent,
        std::shared_ptr<const MediaStorageReleaseNotification> notification) noexcept;
    std::mutex& ledgerMutex() const noexcept;
    void release(std::size_t bytes) noexcept;

    mutable std::mutex m_mutex;
    const std::shared_ptr<MediaStorageBudget<Domain>> m_parent;
    std::optional<MediaStorageLease<Domain>> m_backing;
    MediaByteReservationCounter m_storage;
    std::size_t m_highWaterBytes = 0;
    const std::shared_ptr<const MediaStorageReleaseNotification> m_releaseNotification;
    bool m_pressureArmed = false;
};

template <typename Domain>
MediaStorageLease<Domain>::MediaStorageLease(
    std::shared_ptr<MediaStorageBudget<Domain>> owner,
    std::size_t bytes) noexcept
    : m_owner(std::move(owner)), m_bytes(bytes)
{
}

template <typename Domain>
MediaStorageLease<Domain>::~MediaStorageLease()
{
    release();
}

template <typename Domain>
MediaStorageLease<Domain>::MediaStorageLease(
    MediaStorageLease<Domain>&& other) noexcept
    : m_owner(std::move(other.m_owner)), m_bytes(std::exchange(other.m_bytes, 0))
{
}

template <typename Domain>
MediaStorageLease<Domain>& MediaStorageLease<Domain>::operator=(
    MediaStorageLease<Domain>&& other) noexcept
{
    if (this != &other) {
        release();
        m_owner = std::move(other.m_owner);
        m_bytes = std::exchange(other.m_bytes, 0);
    }
    return *this;
}

template <typename Domain>
::media::Status MediaStorageLease<Domain>::shrinkTo(std::size_t bytes)
{
    if (!m_owner || bytes == 0 || bytes > m_bytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "storage budget settlement must retain a positive admitted allocation"));
    }
    if (bytes != m_bytes) {
        const auto returnedBytes = m_bytes - bytes;
        m_bytes = bytes;
        m_owner->release(returnedBytes);
    }
    return ::media::Status::success();
}

template <typename Domain>
::media::Result<MediaStorageLease<Domain>>
MediaStorageLease<Domain>::split(std::size_t bytes)
{
    using Result = ::media::Result<MediaStorageLease<Domain>>;
    if (!m_owner || bytes == 0 || bytes > m_bytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Storage split requires positive credit within an owned reservation"));
    }
    if (bytes == m_bytes) return Result::success(std::move(*this));
    MediaStorageLease<Domain> portion(m_owner, bytes);
    m_bytes -= bytes;
    return Result::success(std::move(portion));
}

template <typename Domain>
void MediaStorageLease<Domain>::release() noexcept
{
    auto owner = std::move(m_owner);
    const auto bytes = std::exchange(m_bytes, 0);
    if (owner && bytes != 0) {
        owner->release(bytes);
        owner.reset();
    }
}

template <typename Domain>
MediaStorageBudget<Domain>::MediaStorageBudget(
    std::size_t capacity, std::shared_ptr<MediaStorageBudget<Domain>> parent,
    std::shared_ptr<const MediaStorageReleaseNotification> notification) noexcept
    : m_parent(std::move(parent)), m_storage(capacity),
      m_releaseNotification(std::move(notification))
{
}

template <typename Domain>
::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>>
MediaStorageBudget<Domain>::create(std::size_t capacity,
    const MediaStorageScope<Domain>& scope,
    const MediaStorageReleaseObservation& observation)
{
    using Result = ::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>>;
    if (capacity == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "storage budget requires a positive planned capacity"));
    }
    std::shared_ptr<const MediaStorageReleaseNotification> notification;
    if (const auto* observed = std::get_if<
            std::shared_ptr<const MediaStorageReleaseNotification>>(&observation)) {
        if (!*observed) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Storage release notification is null"));
        notification = *observed;
    }
    std::shared_ptr<MediaStorageBudget<Domain>> parent;
    if (const auto* shared = std::get_if<MediaSharedStorage<Domain>>(&scope)) {
        if (!shared->root || shared->root->m_parent) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "shared storage budget requires a root budget, not a child or missing owner"));
        }
        parent = shared->root;
        if (notification && notification != parent->m_releaseNotification)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Shared storage must inherit the root notification domain"));
        notification = parent->m_releaseNotification;
    }
    try {
        return Result::success(std::shared_ptr<MediaStorageBudget<Domain>>(
            new MediaStorageBudget<Domain>(capacity, std::move(parent), std::move(notification))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "storage budget allocation failed"));
    }
}

template <typename Domain>
::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>>
MediaStorageBudget<Domain>::fromReservation(
    MediaStorageLease<Domain> reservation,
    const MediaStorageReleaseObservation& observation)
{
    using Result = ::media::Result<std::shared_ptr<MediaStorageBudget<Domain>>>;
    if (!reservation || !reservation.bytes())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Guaranteed storage budget requires an owned positive reservation"));
    std::shared_ptr<const MediaStorageReleaseNotification> notification;
    if (const auto* observed = std::get_if<
            std::shared_ptr<const MediaStorageReleaseNotification>>(&observation)) {
        if (!*observed) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Storage release notification is null"));
        notification = *observed;
    }
    try {
        // Charge the control/object to the original owner, never to the new
        // budget itself. Its block retains credit through the last weak owner.
        auto budget = std::allocate_shared<MediaStorageBudget<Domain>>(
            BootstrapAllocator{reservation.m_owner}, ConstructionKey{},
            reservation.bytes(), std::move(notification));
        budget->m_backing.emplace(std::move(reservation));
        return Result::success(std::move(budget));
    } catch (const MediaBudgetBootstrapFailure& failure) {
        return Result::failure(failure.error);
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Guaranteed storage budget allocation failed"));
    }
}

template <typename Domain>
::media::Result<MediaStorageLease<Domain>>
MediaStorageBudget<Domain>::reserve(std::size_t bytes, MediaStorageAdmission admission)
{
    using Result = ::media::Result<MediaStorageLease<Domain>>;
    std::scoped_lock lock(ledgerMutex());
    if (bytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "storage budget reservation must be positive"));
    }
    if (admission != MediaStorageAdmission::RequiredAllocation &&
        admission != MediaStorageAdmission::Backpressure) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "storage budget reservation requires a valid admission policy"));
    }
    if (admission == MediaStorageAdmission::Backpressure) {
        auto& root = m_parent ? *m_parent : *this;
        if (bytes > m_storage.capacity() ||
            (m_parent && bytes > m_parent->m_storage.capacity())) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "storage request exceeds the admitted capacity"));
        }
        if (bytes > m_storage.capacity() - m_storage.retainedBytes() ||
            (m_parent && bytes > m_parent->m_storage.capacity() -
                m_parent->m_storage.retainedBytes())) {
            if (!root.m_releaseNotification) return Result::failure(
                ::media::ErrorInfo::notInitialized(
                    "Storage backpressure requires a frozen notification domain"));
            root.m_pressureArmed = true;
            return Result::failure(::media::ErrorInfo::wouldBlock(
                "storage admission is waiting for retained allocations to be released"));
        }
    }
    if (m_parent && bytes > m_parent->m_storage.capacity() - m_parent->m_storage.retainedBytes()) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "storage budget reservation exceeds the shared domain capacity"));
    }
    if (!m_storage.retain(bytes)) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "storage budget reservation exceeds the admitted capacity"));
    }
    if (m_parent) {
        const bool retained = m_parent->m_storage.retain(bytes);
        assert(retained);
        (void)retained;
        m_parent->m_highWaterBytes = (std::max)(
            m_parent->m_highWaterBytes, m_parent->m_storage.retainedBytes());
    }
    m_highWaterBytes = (std::max)(m_highWaterBytes, m_storage.retainedBytes());
    return Result::success(MediaStorageLease<Domain>(this->shared_from_this(), bytes));
}

template <typename Domain>
void MediaStorageBudget<Domain>::release(std::size_t bytes) noexcept
{
    std::shared_ptr<const MediaStorageReleaseNotification> notification;
    {
        std::scoped_lock lock(ledgerMutex());
        const bool released = m_storage.release(bytes);
        assert(released);
        (void)released;
        if (m_parent) {
            const bool parentReleased = m_parent->m_storage.release(bytes);
            assert(parentReleased);
            (void)parentReleased;
        }
        auto& root = m_parent ? *m_parent : *this;
        if (std::exchange(root.m_pressureArmed, false))
            notification = root.m_releaseNotification;
    }
    if (notification) notification->notify();
}

template <typename Domain>
std::mutex& MediaStorageBudget<Domain>::ledgerMutex() const noexcept
{
    // A root and all its direct children share one ledger critical section.
    // No nested locks or partially visible two-level reservations are possible.
    return m_parent ? m_parent->m_mutex : m_mutex;
}

template <typename Domain>
MediaStorageSnapshot<Domain> MediaStorageBudget<Domain>::snapshot() const noexcept
{
    std::scoped_lock lock(ledgerMutex());
    return {m_storage.capacity(), m_storage.retainedBytes(), m_highWaterBytes};
}

template <typename Domain>
::media::Result<MediaStoragePartitionSnapshot<Domain>>
MediaStorageBudget<Domain>::snapshotPartition(
    std::span<const std::shared_ptr<MediaStorageBudget<Domain>>> children,
    std::span<MediaStorageSnapshot<Domain>> snapshots)
{
    using Result = ::media::Result<MediaStoragePartitionSnapshot<Domain>>;
    if (children.empty() || children.size() != snapshots.size() ||
        !children.front() || !children.front()->m_parent)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Storage partition requires direct children and matching snapshot storage"));
    const auto root = children.front()->m_parent;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i] || children[i]->m_parent != root)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Storage partition requires one shared root owner"));
        for (std::size_t j = 0; j < i; ++j)
            if (children[i] == children[j])
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Storage partition rejects duplicate children"));
    }
    MediaStoragePartitionSnapshot<Domain> partition{root, {}, 0, 0};
    {
        std::scoped_lock lock(root->m_mutex);
        partition.budget = {root->m_storage.capacity(),
            root->m_storage.retainedBytes(), root->m_highWaterBytes};
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto& child = *children[i];
            snapshots[i] = {child.m_storage.capacity(),
                child.m_storage.retainedBytes(), child.m_highWaterBytes};
            // Direct-child charges are disjoint and already included in root.
            assert(snapshots[i].retainedBytes <=
                partition.budget.retainedBytes - partition.selectedRetainedBytes);
            partition.selectedRetainedBytes += snapshots[i].retainedBytes;
        }
        partition.otherRetainedBytes = partition.budget.retainedBytes -
            partition.selectedRetainedBytes;
    }
    return Result::success(std::move(partition));
}

} // namespace media::ffmpeg::graph
