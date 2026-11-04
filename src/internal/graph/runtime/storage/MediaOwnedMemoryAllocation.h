#pragma once
#include <cstddef>
#include <memory_resource>
#include <utility>

namespace media::ffmpeg::graph {
template<class Budget, class Lease> class MediaBudgetMemoryResource;

// Owns an unconstructed PMR allocation. The budget allocation header retains
// the resource; consuming this guard transfers responsibility for deallocation.
class MediaOwnedMemoryAllocation final {
public:
    MediaOwnedMemoryAllocation(const MediaOwnedMemoryAllocation&) = delete;
    MediaOwnedMemoryAllocation& operator=(const MediaOwnedMemoryAllocation&) = delete;
    MediaOwnedMemoryAllocation(MediaOwnedMemoryAllocation&& other) noexcept
        : m_pointer(std::exchange(other.m_pointer, nullptr)), m_bytes(other.m_bytes),
          m_alignment(other.m_alignment), m_resource(other.m_resource) {}
    MediaOwnedMemoryAllocation& operator=(MediaOwnedMemoryAllocation&&) = delete;
    ~MediaOwnedMemoryAllocation() noexcept
    { if (m_pointer) m_resource->deallocate(m_pointer, m_bytes, m_alignment); }
    explicit operator bool() const noexcept { return m_pointer != nullptr; }
    std::size_t bytes() const noexcept { return m_bytes; }
    std::size_t alignment() const noexcept { return m_alignment; }
    std::pmr::memory_resource* resource() const noexcept { return m_resource; }
    void* data() const noexcept { return m_pointer; }
    void* release() noexcept { return std::exchange(m_pointer, nullptr); }
private:
    template<class Budget, class Lease> friend class MediaBudgetMemoryResource;
    MediaOwnedMemoryAllocation(void* pointer, std::size_t bytes,
        std::size_t alignment, std::pmr::memory_resource& resource) noexcept
        : m_pointer(pointer), m_bytes(bytes), m_alignment(alignment), m_resource(&resource) {}
    void* m_pointer;
    std::size_t m_bytes;
    std::size_t m_alignment;
    std::pmr::memory_resource* m_resource;
};
} // namespace media::ffmpeg::graph
