#pragma once
#include "media_transcode/Result.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

// Shared mechanics for resource payloads and the resource bootstrap control.
// Owner is the resource for payloads, but only the budget during bootstrap.
template<class Owner, class Lease>
class MediaBudgetAllocationBlock final {
    struct Header final { Owner owner; Lease reservation; };
public:
    struct Layout final { std::size_t alignment; std::size_t offset; std::size_t bytes; };
    static std::optional<Layout> layout(std::size_t bytes, std::size_t alignment) noexcept
    {
        if (!alignment || (alignment & (alignment - 1))) return std::nullopt;
        const auto blockAlignment = (std::max)(alignment, alignof(Header));
        const auto maximum = (std::numeric_limits<std::size_t>::max)();
        if (sizeof(Header) > maximum - (blockAlignment - 1)) return std::nullopt;
        const auto offset = (sizeof(Header) + blockAlignment - 1) & ~(blockAlignment - 1);
        if (bytes > maximum - offset) return std::nullopt;
        return Layout{blockAlignment, offset, offset + bytes};
    }
    static ::media::Result<void*> allocate(const Layout& geometry, Owner owner, Lease reservation)
    {
        static_assert(std::is_nothrow_move_constructible_v<Owner>);
        static_assert(std::is_nothrow_move_constructible_v<Lease>);
        using Result = ::media::Result<void*>;
        try {
            auto* block = static_cast<std::byte*>(::operator new(
                geometry.bytes, std::align_val_t(geometry.alignment)));
            std::construct_at(reinterpret_cast<Header*>(block),
                Header{std::move(owner), std::move(reservation)});
            return Result::success(block + geometry.offset);
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed("Budget storage allocation failed"));
        }
    }
    static void deallocate(void* pointer, const Layout& geometry) noexcept
    {
        auto* block = static_cast<std::byte*>(pointer) - geometry.offset;
        auto* header = reinterpret_cast<Header*>(block);
        auto owner = std::move(header->owner);
        {
            auto reservation = std::move(header->reservation);
            std::destroy_at(header);
            ::operator delete(block, std::align_val_t(geometry.alignment));
            // Native backing disappears before credit is returned; owner last.
        }
    }
};

} // namespace media::ffmpeg::graph
