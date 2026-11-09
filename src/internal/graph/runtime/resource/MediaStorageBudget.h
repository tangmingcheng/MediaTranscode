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
    // Actual lease reservations, including any fromReservation backing lease.
    // Neither this nor chargedBytes measures allocator bytes, RSS or VRAM.
    std::size_t actualRetainedBytes;
    std::size_t guaranteedBytes;
    std::size_t chargedBytes;
    std::size_t actualHighWaterBytes;
    std::size_t chargedHighWaterBytes;
    bool sealed;
};

template <typename Domain>
struct MediaStoragePartitionSnapshot final {
    std::shared_ptr<const MediaStorageBudget<Domain>> root;
    MediaStorageSnapshot<Domain> budget;
    std::size_t selectedActualBytes;
    std::size_t selectedChargedBytes;
    std::size_t otherActualBytes;
    std::size_t otherChargedBytes;
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
    ~MediaStorageBudget() noexcept;
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
    // All-or-nothing, one-time guarantees for distinct direct children. Zero is
    // a valid sealed capacity for an empty child. No allocation/callback under lock.
    static ::media::Status sealPartition(
        std::span<const std::shared_ptr<MediaStorageBudget<Domain>>> children,
        std::span<const std::size_t> capacities);
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
    std::size_t capacityLocked() const noexcept
    { return m_sealed ? m_guaranteedBytes : m_storage.capacity(); }
    std::size_t chargedLocked() const noexcept
    { return m_storage.retainedBytes() + m_unusedGuaranteedBytes; }
    MediaStorageSnapshot<Domain> snapshotLocked() const noexcept
    {
        return {capacityLocked(), m_storage.retainedBytes(), m_guaranteedBytes,
            chargedLocked(), m_actualHighWaterBytes, m_chargedHighWaterBytes, m_sealed};
    }
    void updateHighWaterLocked() noexcept
    {
        m_actualHighWaterBytes = (std::max)(m_actualHighWaterBytes, m_storage.retainedBytes());
        m_chargedHighWaterBytes = (std::max)(m_chargedHighWaterBytes, chargedLocked());
    }

    mutable std::mutex m_mutex;
    const std::shared_ptr<MediaStorageBudget<Domain>> m_parent;
    std::optional<MediaStorageLease<Domain>> m_backing;
    MediaByteReservationCounter m_storage;
    std::size_t m_actualHighWaterBytes = 0;
    std::size_t m_chargedHighWaterBytes = 0;
    // Root values aggregate direct children; sealed child values describe itself.
    std::size_t m_guaranteedBytes = 0;
    std::size_t m_unusedGuaranteedBytes = 0;
    bool m_sealed = false;
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
MediaStorageBudget<Domain>::~MediaStorageBudget() noexcept
{
    if (!m_parent || !m_sealed) return;
    std::shared_ptr<const MediaStorageReleaseNotification> notification;
    {
        std::scoped_lock lock(m_parent->m_mutex);
        // Every outstanding lease owns this child, so destruction implies none.
        assert(m_storage.retainedBytes() == 0);
        assert(m_unusedGuaranteedBytes == m_guaranteedBytes);
        assert(m_parent->m_unusedGuaranteedBytes >= m_unusedGuaranteedBytes);
        assert(m_parent->m_guaranteedBytes >= m_guaranteedBytes);
        m_parent->m_unusedGuaranteedBytes -= m_unusedGuaranteedBytes;
        m_parent->m_guaranteedBytes -= m_guaranteedBytes;
        if (std::exchange(m_parent->m_pressureArmed, false))
            notification = m_parent->m_releaseNotification;
    }
    if (notification) notification->notify();
}

template <typename Domain>
::media::Status MediaStorageBudget<Domain>::sealPartition(
    std::span<const std::shared_ptr<MediaStorageBudget<Domain>>> children,
    std::span<const std::size_t> capacities)
{
    if (children.empty() || children.size() != capacities.size() ||
        !children.front() || !children.front()->m_parent)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Storage sealing requires direct children and matching capacities"));
    const auto root = children.front()->m_parent;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i] || children[i]->m_parent != root)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Storage sealing requires one shared root"));
        for (std::size_t j = 0; j < i; ++j)
            if (children[i] == children[j])
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Storage sealing rejects duplicate children"));
    }
    const char* error = nullptr;
    {
        std::scoped_lock lock(root->m_mutex);
        std::size_t unused = 0;
        std::size_t guarantees = 0;
        const auto remaining = root->m_storage.capacity() - root->chargedLocked();
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto& child = *children[i];
            const auto capacity = capacities[i];
            if (child.m_sealed || capacity < child.m_storage.retainedBytes() ||
                capacity > child.m_storage.capacity()) {
                error = "Storage sealing requires an unsealed child and capacity covering held credit within its original limit";
                break;
            }
            const auto delta = capacity - child.m_storage.retainedBytes();
            if (delta > remaining - unused) {
                error = "Storage guarantees exceed shared deployment capacity";
                break;
            }
            unused += delta;
            if (capacity > root->m_storage.capacity() - root->m_guaranteedBytes - guarantees) {
                error = "Storage guarantees exceed the root guarantee capacity";
                break;
            }
            guarantees += capacity;
        }
        if (!error) {
            assert(guarantees <= root->m_storage.capacity() - root->m_guaranteedBytes);
            for (std::size_t i = 0; i < children.size(); ++i) {
                auto& child = *children[i];
                child.m_guaranteedBytes = capacities[i];
                child.m_unusedGuaranteedBytes = capacities[i] - child.m_storage.retainedBytes();
                child.m_sealed = true;
                child.updateHighWaterLocked();
            }
            root->m_guaranteedBytes += guarantees;
            root->m_unusedGuaranteedBytes += unused;
            root->updateHighWaterLocked();
        }
    }
    if (error) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(error));
    return ::media::Status::success();
}

template <typename Domain>
::media::Result<MediaStorageLease<Domain>>
MediaStorageBudget<Domain>::reserve(std::size_t bytes, MediaStorageAdmission admission)
{
    using Result = ::media::Result<MediaStorageLease<Domain>>;
    if (bytes == 0) return Result::failure(::media::ErrorInfo::invalidArgument(
        "storage budget reservation must be positive"));
    if (admission != MediaStorageAdmission::RequiredAllocation &&
        admission != MediaStorageAdmission::Backpressure)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "storage budget reservation requires a valid admission policy"));
    enum class Failure { None, Oversize, Pressure, Unobserved, Allocation };
    auto failure = Failure::None;
    {
        std::scoped_lock lock(ledgerMutex());
        auto& root = m_parent ? *m_parent : *this;
        const bool oversize = bytes > capacityLocked() || bytes > root.m_storage.capacity();
        const bool unavailable = oversize ||
            bytes > capacityLocked() - m_storage.retainedBytes() ||
            (!m_sealed && bytes > root.m_storage.capacity() - root.chargedLocked());
        if (unavailable) {
            if (admission == MediaStorageAdmission::RequiredAllocation)
                failure = Failure::Allocation;
            else if (oversize) failure = Failure::Oversize;
            else if (!root.m_releaseNotification) failure = Failure::Unobserved;
            else {
                root.m_pressureArmed = true;
                failure = Failure::Pressure;
            }
        } else {
            const bool retained = m_storage.retain(bytes);
            assert(retained);
            (void)retained;
            if (m_parent) {
                const bool parentRetained = root.m_storage.retain(bytes);
                assert(parentRetained);
                (void)parentRetained;
                if (m_sealed) {
                    assert(bytes <= m_unusedGuaranteedBytes);
                    m_unusedGuaranteedBytes -= bytes;
                    root.m_unusedGuaranteedBytes -= bytes;
                }
                root.updateHighWaterLocked();
            }
            updateHighWaterLocked();
        }
    }
    switch (failure) {
    case Failure::Oversize: return Result::failure(::media::ErrorInfo::invalidArgument(
        "storage request exceeds the admitted capacity"));
    case Failure::Pressure: return Result::failure(::media::ErrorInfo::wouldBlock(
        "storage admission is waiting for retained allocations to be released"));
    case Failure::Unobserved: return Result::failure(::media::ErrorInfo::notInitialized(
        "Storage backpressure requires a frozen notification domain"));
    case Failure::Allocation: return Result::failure(::media::ErrorInfo::allocationFailed(
        "storage budget reservation exceeds the admitted capacity"));
    case Failure::None: break;
    }
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
            if (m_sealed) {
                m_unusedGuaranteedBytes += bytes;
                m_parent->m_unusedGuaranteedBytes += bytes;
            }
        }
        auto& root = m_parent ? *m_parent : *this;
        // Also notify when sealed release leaves root charged bytes unchanged:
        // that child's blocked producer may now have usable credit.
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
    return snapshotLocked();
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
    MediaStoragePartitionSnapshot<Domain> partition{root, {}, 0, 0, 0, 0};
    {
        std::scoped_lock lock(root->m_mutex);
        partition.budget = root->snapshotLocked();
        for (std::size_t i = 0; i < children.size(); ++i) {
            snapshots[i] = children[i]->snapshotLocked();
            assert(snapshots[i].actualRetainedBytes <=
                partition.budget.actualRetainedBytes - partition.selectedActualBytes);
            assert(snapshots[i].chargedBytes <=
                partition.budget.chargedBytes - partition.selectedChargedBytes);
            partition.selectedActualBytes += snapshots[i].actualRetainedBytes;
            partition.selectedChargedBytes += snapshots[i].chargedBytes;
        }
        partition.otherActualBytes = partition.budget.actualRetainedBytes - partition.selectedActualBytes;
        partition.otherChargedBytes = partition.budget.chargedBytes - partition.selectedChargedBytes;
    }
    return Result::success(std::move(partition));
}

} // namespace media::ffmpeg::graph
