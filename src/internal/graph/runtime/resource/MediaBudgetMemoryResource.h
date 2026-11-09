#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/resource/MediaStorageBudget.h"
#include "internal/graph/runtime/resource/MediaBudgetBootstrapReadback.h"
#include "internal/graph/runtime/storage/MediaOwnedMemoryAllocation.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaPreparationStorageDomain;

// Budget supplies reserve(bytes, admission) -> Result<Lease>. Each allocation retains its
// typed budget lease and the resource until deallocation. Different budget
// domains share allocation mechanics without sharing their admitted capacity.
// Accounts C++ allocation requests, header and padding, not heap internals/RSS.
template <typename Budget, typename Lease>
class MediaBudgetMemoryResource final
    : public std::pmr::memory_resource,
      public std::enable_shared_from_this<MediaBudgetMemoryResource<Budget, Lease>> {
    static_assert(std::is_same_v<decltype(std::declval<Budget&>().reserve(std::size_t{}, MediaStorageAdmission::RequiredAllocation)),
        ::media::Result<Lease>>);
    static_assert(std::is_nothrow_move_constructible_v<Lease>);
    static_assert(std::is_nothrow_destructible_v<Lease>);
    using Block = MediaBudgetAllocationBlock<std::shared_ptr<MediaBudgetMemoryResource>, Lease>;
    class ConstructionKey final {
        friend class MediaBudgetMemoryResource;
        ConstructionKey() = default;
    };
public:
    using BootstrapAllocator = MediaBudgetBootstrapAllocator<MediaBudgetMemoryResource, Budget, Lease>;
    // Public for allocator_traits::construct; callers cannot name the private key.
    MediaBudgetMemoryResource(ConstructionKey, std::shared_ptr<Budget> budget) noexcept
        : m_budget(std::move(budget)) {}
    static ::media::Result<std::size_t> bootstrapAllocationFootprint(
        std::size_t bytes, std::size_t alignment)
    { return MediaBudgetBootstrapReadback<Budget, Lease>::allocationFootprint(bytes, alignment); }
    static ::media::Result<std::size_t> bootstrapStorageBytes()
    {
        return MediaBudgetBootstrapReadback<Budget, Lease>::measure(
            [](std::pmr::memory_resource& readback) {
                return std::allocate_shared<MediaBudgetMemoryResource>(
                    BootstrapAllocator{readback}, ConstructionKey{}, std::shared_ptr<Budget>{});
            });
    }
    static ::media::Result<std::shared_ptr<MediaBudgetMemoryResource>> create(
        std::shared_ptr<Budget> budget)
    {
        using Result = ::media::Result<std::shared_ptr<MediaBudgetMemoryResource>>;
        if (!budget) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Memory resource requires an admitted storage budget"));
        try {
            if constexpr (std::is_same_v<Budget, MediaStorageBudget<MediaPreparationStorageDomain>>) {
                return Result::success(std::allocate_shared<MediaBudgetMemoryResource>(
                    BootstrapAllocator{budget}, ConstructionKey{}, budget));
            } else {
                return Result::success(std::shared_ptr<MediaBudgetMemoryResource>(
                    new MediaBudgetMemoryResource(ConstructionKey{}, std::move(budget))));
            }
        } catch (const MediaBudgetBootstrapFailure& failure) {
            return Result::failure(failure.error);
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "Budget memory resource allocation failed"));
        }
    }

    static ::media::Result<std::size_t> allocationFootprint(
        std::size_t bytes, std::size_t alignment)
    {
        const auto layout = Block::layout(bytes, alignment);
        if (!layout) return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::allocationFailed("Budget allocation layout is invalid or overflows"));
        return ::media::Result<std::size_t>::success(layout->bytes);
    }

    // One reservation is transferred directly into the real allocation header.
    // Typed callers retain pressure errors; standard PMR callers still throw.
    ::media::Result<void*> allocateWithAdmission(
        std::size_t bytes, std::size_t alignment, MediaStorageAdmission admission)
    {
        using Result = ::media::Result<void*>;
        const auto layout = Block::layout(bytes, alignment);
        if (!layout) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Budget allocation layout is invalid or overflows"));
        auto reservation = m_budget->reserve(layout->bytes, admission);
        if (!reservation) return Result::failure(reservation.error());
        auto allocated = allocateFromReservation(bytes, alignment, std::move(reservation).value());
        if (!allocated) return Result::failure(allocated.error());
        return Result::success(allocated.value().release());
    }

    // Consume one exact portion of a previously admitted batch. No second
    // reserve occurs here; native allocation failure is terminal, not pressure.
    // The by-value lease is consumed on success and returned on every failure.
    ::media::Result<MediaOwnedMemoryAllocation> allocateFromReservation(
        std::size_t bytes, std::size_t alignment, Lease reservation)
    {
        using Result = ::media::Result<MediaOwnedMemoryAllocation>;
        const auto layout = Block::layout(bytes, alignment);
        if (!layout || !reservation.belongsTo(*m_budget) ||
            reservation.bytes() != layout->bytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Budget allocation requires its exact footprint and original budget lease"));
        }
        auto allocated = Block::allocate(*layout, this->shared_from_this(), std::move(reservation));
        if (!allocated) return Result::failure(allocated.error());
        return Result::success(MediaOwnedMemoryAllocation(allocated.value(), bytes, alignment, *this));
    }

    ::media::Result<Lease> reserveStorage(std::size_t footprint, MediaStorageAdmission admission)
    { return m_budget->reserve(footprint, admission); }

    // Budget identity is distinct from PMR allocation-owner identity: separate
    // resource wrappers may charge the same ledger without being interchangeable.
    bool usesBudget(const Budget& budget) const noexcept
    { return m_budget.get() == &budget; }

    MediaBudgetMemoryResource(const MediaBudgetMemoryResource&) = delete;
    MediaBudgetMemoryResource& operator=(const MediaBudgetMemoryResource&) = delete;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        auto allocated = allocateWithAdmission(
            bytes, alignment, MediaStorageAdmission::RequiredAllocation);
        if (!allocated) throw std::bad_alloc();
        return allocated.value();
    }
    void do_deallocate(void* pointer, std::size_t bytes,
        std::size_t alignment) override
    {
        // PMR requires the original allocation's size, alignment and pointer.
        const auto layout = Block::layout(bytes, alignment);
        Block::deallocate(pointer, *layout);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }
    std::shared_ptr<Budget> m_budget;
};

} // namespace media::ffmpeg::graph
