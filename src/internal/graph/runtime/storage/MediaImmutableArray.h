#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace media::ffmpeg::graph {

// Shared read-only element storage. The requested element payload is exact;
// allocator bookkeeping and shared_ptr control blocks are not included.
template<class T>
class MediaImmutableArray final {
public:
    MediaImmutableArray() noexcept = default;

    static ::media::Result<MediaImmutableArray> copy(std::span<const T> values)
    {
        if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T))
            return ::media::Result<MediaImmutableArray>::failure(
                ::media::ErrorInfo::invalidArgument("Immutable array size overflow"));
        if (values.empty())
            return ::media::Result<MediaImmutableArray>::success(MediaImmutableArray{});
        try {
            std::allocator<T> allocator;
            T* data = allocator.allocate(values.size());
            try {
                // uninitialized_copy destroys constructed elements on failure.
                std::uninitialized_copy(values.begin(), values.end(), data);
            } catch (...) {
                allocator.deallocate(data, values.size());
                throw;
            }
            // shared_ptr invokes this deleter even if its control allocation fails.
            std::shared_ptr<const T[]> owner(data, [count = values.size()](const T* pointer) noexcept {
                auto* mutablePointer = const_cast<T*>(pointer);
                std::destroy_n(mutablePointer, count);
                std::allocator<T>{}.deallocate(mutablePointer, count);
            });
            return ::media::Result<MediaImmutableArray>::success(
                MediaImmutableArray(std::move(owner), values.size()));
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaImmutableArray>::failure(
                ::media::ErrorInfo::allocationFailed("Immutable array allocation failed"));
        }
    }

    std::span<const T> view() const noexcept { return {m_owner.get(), m_size}; }
    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    std::size_t storageBytes() const noexcept { return m_size * sizeof(T); }
    const T& operator[](std::size_t index) const noexcept { return m_owner[index]; }
    const T& front() const noexcept { return view().front(); }
    const T& back() const noexcept { return view().back(); }
    auto begin() const noexcept { return view().begin(); }
    auto end() const noexcept { return view().end(); }

    MediaImmutableArray(const MediaImmutableArray&) noexcept = default;
    MediaImmutableArray& operator=(const MediaImmutableArray&) noexcept = default;
    MediaImmutableArray(MediaImmutableArray&& other) noexcept
        : m_owner(std::move(other.m_owner)), m_size(std::exchange(other.m_size, 0)) {}
    MediaImmutableArray& operator=(MediaImmutableArray&& other) noexcept
    {
        if (this != &other) {
            m_owner = std::move(other.m_owner);
            m_size = std::exchange(other.m_size, 0);
        }
        return *this;
    }

private:
    MediaImmutableArray(std::shared_ptr<const T[]> owner, std::size_t size) noexcept
        : m_owner(std::move(owner)), m_size(size) {}
    std::shared_ptr<const T[]> m_owner;
    std::size_t m_size = 0;
};

} // namespace media::ffmpeg::graph
