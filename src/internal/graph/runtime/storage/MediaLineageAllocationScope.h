#pragma once

#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"

#include <limits>
#include <memory_resource>
#include <new>
#include <variant>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaRuntimeLineageAllocation final {};
struct MediaPreparedLineageAllocation final {
    std::shared_ptr<MediaPreparationMemoryResource> resource;
};
using MediaLineageAllocationScope = std::variant<
    MediaRuntimeLineageAllocation, MediaPreparedLineageAllocation>;

// Shared control blocks and containers retain the resource through allocator
// copies. Container assignment transfers the source domain instead of silently
// allocating through a destination default resource.
template<class T>
class MediaLineageAllocator {
public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    explicit MediaLineageAllocator(MediaLineageAllocationScope scope) noexcept
        : m_scope(std::move(scope)) {}
    MediaLineageAllocator(const MediaLineageAllocator&) noexcept = default;
    MediaLineageAllocator& operator=(const MediaLineageAllocator&) noexcept = default;
    // A moved-from container may retain a debug iterator proxy allocated with
    // its allocator. Moving therefore preserves the source allocation domain.
    MediaLineageAllocator(MediaLineageAllocator&& other) noexcept
        : m_scope(other.m_scope) {}
    MediaLineageAllocator& operator=(MediaLineageAllocator&& other) noexcept
    { m_scope = other.m_scope; return *this; }
    template<class U>
    MediaLineageAllocator(const MediaLineageAllocator<U>& other) noexcept
        : m_scope(other.scope()) {}

    T* allocate(std::size_t count)
    {
        if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T))
            throw std::bad_alloc();
        return static_cast<T*>(resource()->allocate(count * sizeof(T), alignof(T)));
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    { resource()->deallocate(pointer, count * sizeof(T), alignof(T)); }
    const MediaLineageAllocationScope& scope() const noexcept { return m_scope; }
    template<class U>
    bool operator==(const MediaLineageAllocator<U>& other) const noexcept
    { return resource()->is_equal(*other.resource()); }
    std::pmr::memory_resource* resource() const noexcept
    {
        if (const auto* prepared = std::get_if<MediaPreparedLineageAllocation>(&m_scope))
            return prepared->resource.get();
        return std::pmr::new_delete_resource();
    }
private:
    MediaLineageAllocationScope m_scope;
};

} // namespace media::ffmpeg::graph
