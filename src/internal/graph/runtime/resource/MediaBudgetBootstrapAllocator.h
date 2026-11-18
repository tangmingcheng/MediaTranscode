#pragma once
#include "internal/graph/runtime/resource/MediaBudgetAllocationBlock.h"
#include "internal/graph/runtime/resource/MediaStorageAdmission.h"
#include <limits>
#include <memory>
#include <memory_resource>
#include <utility>

namespace media::ffmpeg::graph {

// Retain the original budget rejection across allocate_shared's allocator API.
struct MediaBudgetBootstrapFailure final : std::bad_alloc {
    explicit MediaBudgetBootstrapFailure(::media::ErrorInfo value) : error(std::move(value)) {}
    ::media::ErrorInfo error;
};

template<class T, class Budget, class Lease>
class MediaBudgetBootstrapAllocator final {
    using Block = MediaBudgetAllocationBlock<std::shared_ptr<Budget>, Lease>;
public:
    using value_type = T;
    template<class U> struct rebind { using other = MediaBudgetBootstrapAllocator<U, Budget, Lease>; };
    explicit MediaBudgetBootstrapAllocator(std::shared_ptr<Budget> budget) noexcept : m_budget(std::move(budget)) {}
    explicit MediaBudgetBootstrapAllocator(std::pmr::memory_resource& readback) noexcept : m_readback(&readback) {}
    MediaBudgetBootstrapAllocator(const MediaBudgetBootstrapAllocator&) noexcept = default;
    MediaBudgetBootstrapAllocator& operator=(const MediaBudgetBootstrapAllocator&) noexcept = default;
    MediaBudgetBootstrapAllocator(MediaBudgetBootstrapAllocator&& other) noexcept : m_budget(other.m_budget), m_readback(other.m_readback) {}
    MediaBudgetBootstrapAllocator& operator=(MediaBudgetBootstrapAllocator&& other) noexcept
    { m_budget = other.m_budget; m_readback = other.m_readback; return *this; }
    template<class U>
    MediaBudgetBootstrapAllocator(const MediaBudgetBootstrapAllocator<U, Budget, Lease>& other) noexcept
        : m_budget(other.budget()), m_readback(other.readback()) {}
    T* allocate(std::size_t count)
    {
        if (!count || count > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) throw std::bad_alloc();
        if (m_readback) return static_cast<T*>(m_readback->allocate(count * sizeof(T), alignof(T)));
        auto geometry = Block::layout(count * sizeof(T), alignof(T));
        if (!geometry) throw std::bad_alloc();
        auto reservation = m_budget->reserve(geometry->bytes, MediaStorageAdmission::RequiredAllocation);
        if (!reservation) throw MediaBudgetBootstrapFailure{reservation.error()};
        auto allocated = Block::allocate(*geometry, m_budget, std::move(reservation).value());
        if (!allocated) throw MediaBudgetBootstrapFailure{allocated.error()};
        return static_cast<T*>(allocated.value());
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        if (m_readback) m_readback->deallocate(pointer, count * sizeof(T), alignof(T));
        else Block::deallocate(pointer, *Block::layout(count * sizeof(T), alignof(T)));
    }
    const std::shared_ptr<Budget>& budget() const noexcept { return m_budget; }
    std::pmr::memory_resource* readback() const noexcept { return m_readback; }
    template<class U>
    bool operator==(const MediaBudgetBootstrapAllocator<U, Budget, Lease>& other) const noexcept
    { return m_budget == other.budget() && m_readback == other.readback(); }
private:
    // The control allocator outlives the resource object through all weak refs.
    // It never owns the resource, so bootstrap cannot introduce a self-cycle.
    std::shared_ptr<Budget> m_budget;
    std::pmr::memory_resource* m_readback = nullptr;
};

} // namespace media::ffmpeg::graph
