#pragma once

#include "internal/graph/runtime/storage/MediaOwnedMemoryAllocation.h"
#include "media_transcode/Result.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <span>

namespace media::ffmpeg::graph {

// Exactly one allocation request; no growth or allocator-dependent spare capacity.
// The caller keeps the resource alive. Preparation resources additionally pin
// themselves in each allocation until its actual deallocation.
class MediaOwnedByteStorage final {
public:
    static ::media::Result<MediaOwnedByteStorage> fromAllocation(
        MediaOwnedMemoryAllocation allocation);
    MediaOwnedByteStorage(std::pmr::memory_resource& resource, std::size_t bytes)
        : m_data(bytes ? std::pmr::polymorphic_allocator<std::uint8_t>{&resource}.allocate(bytes)
                       : nullptr,
              Deleter{&resource, bytes}) {}
    MediaOwnedByteStorage(std::pmr::memory_resource& resource,
        std::span<const std::uint8_t> source)
        : MediaOwnedByteStorage(resource, source.size())
    {
        if (!source.empty()) std::copy(source.begin(), source.end(), data());
    }
    MediaOwnedByteStorage(const MediaOwnedByteStorage&) = delete;
    MediaOwnedByteStorage& operator=(const MediaOwnedByteStorage&) = delete;
    MediaOwnedByteStorage(MediaOwnedByteStorage&&) noexcept = default;
    MediaOwnedByteStorage& operator=(MediaOwnedByteStorage&&) noexcept = default;

    std::size_t size() const noexcept { return m_data ? m_data.get_deleter().bytes : 0; }
    bool empty() const noexcept { return size() == 0; }
    std::uint8_t* data() noexcept { return m_data.get(); }
    const std::uint8_t* data() const noexcept { return m_data.get(); }
    std::span<std::uint8_t> view() noexcept { return {data(), size()}; }
    std::span<const std::uint8_t> view() const noexcept { return {data(), size()}; }
    auto begin() noexcept { return view().begin(); }
    auto end() noexcept { return view().end(); }
    auto begin() const noexcept { return view().begin(); }
    auto end() const noexcept { return view().end(); }
    std::pmr::memory_resource* resource() const noexcept { return m_data.get_deleter().resource; }

private:
    MediaOwnedByteStorage(std::uint8_t* data, std::size_t bytes,
        std::pmr::memory_resource& resource) noexcept
        : m_data(data, Deleter{&resource, bytes}) {}

    struct Deleter final {
        std::pmr::memory_resource* resource;
        std::size_t bytes;
        void operator()(std::uint8_t* pointer) const noexcept
        { std::pmr::polymorphic_allocator<std::uint8_t>{resource}.deallocate(pointer, bytes); }
    };
    std::unique_ptr<std::uint8_t, Deleter> m_data;
};

inline ::media::Result<MediaOwnedByteStorage>
MediaOwnedByteStorage::fromAllocation(MediaOwnedMemoryAllocation allocation)
{
    using Result = ::media::Result<MediaOwnedByteStorage>;
    if (!allocation || !allocation.bytes() ||
        allocation.alignment() != alignof(std::uint8_t)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Byte storage requires nonempty byte-aligned allocation backing"));
    }
    const auto bytes = allocation.bytes();
    auto& resource = *allocation.resource();
    auto* data = static_cast<std::uint8_t*>(allocation.release());
    return Result::success(MediaOwnedByteStorage(data, bytes, resource));
}

} // namespace media::ffmpeg::graph
