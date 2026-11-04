#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

template<class T>
class MediaLineageObjectDeleter final {
    static_assert(std::is_nothrow_move_constructible_v<MediaLineageAllocator<T>>);
    static_assert(std::is_nothrow_move_assignable_v<MediaLineageAllocator<T>>);
public:
    explicit MediaLineageObjectDeleter(MediaLineageAllocationScope scope)
        : m_allocator(std::move(scope))
    {
        if (!m_allocator.resource())
            throw std::invalid_argument("Lineage unique owner requires its original allocation resource");
    }
    MediaLineageObjectDeleter(MediaLineageObjectDeleter&&) noexcept = default;
    MediaLineageObjectDeleter& operator=(MediaLineageObjectDeleter&&) noexcept = default;
    MediaLineageObjectDeleter(const MediaLineageObjectDeleter&) = delete;
    MediaLineageObjectDeleter& operator=(const MediaLineageObjectDeleter&) = delete;
    void operator()(T* object) const noexcept
    {
        std::pmr::polymorphic_allocator<std::byte>{resource()}.template delete_object<T>(object);
    }
    std::pmr::memory_resource* resource() const noexcept { return m_allocator.resource(); }
private:
    MediaLineageAllocator<T> m_allocator;
};

template<class T>
using MediaLineageUniquePtr = std::unique_ptr<T, MediaLineageObjectDeleter<T>>;

template<class T, class... Arguments>
MediaLineageUniquePtr<T> makeMediaLineageUnique(
    MediaLineageAllocationScope scope, Arguments&&... arguments)
{
    static_assert(std::is_nothrow_move_constructible_v<MediaLineageObjectDeleter<T>>);
    static_assert(std::is_nothrow_move_assignable_v<MediaLineageObjectDeleter<T>>);
    MediaLineageObjectDeleter<T> deleter(std::move(scope));
    auto* object = std::pmr::polymorphic_allocator<std::byte>{deleter.resource()}
        .template new_object<T>(std::forward<Arguments>(arguments)...);
    return MediaLineageUniquePtr<T>(object, std::move(deleter));
}
} // namespace media::ffmpeg::graph
