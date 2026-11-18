#pragma once
#include "internal/graph/runtime/storage/MediaOwnedMemoryAllocation.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <span>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace media::ffmpeg::graph {

// Single-owner exact element backing; no growth and no allocator-dependent spare capacity.
// Ring operations retain no erased references. Allocation bookkeeping is external.
template<class T>
class MediaFixedCapacityStorage final {
public:
    static ::media::Result<MediaFixedCapacityStorage<T>> fromAllocation(MediaOwnedMemoryAllocation allocation);
    MediaFixedCapacityStorage() = default;
    explicit MediaFixedCapacityStorage(std::size_t capacity)
        : MediaFixedCapacityStorage(capacity, *std::pmr::new_delete_resource()) {}
    MediaFixedCapacityStorage(std::size_t capacity, std::pmr::memory_resource& resource)
        : m_resource(&resource), m_capacity(capacity)
    {
        if (capacity > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::length_error("Fixed storage size overflow");
        if (!capacity) return;
        m_data = std::pmr::polymorphic_allocator<T>{m_resource}.allocate(capacity);
    }
    ~MediaFixedCapacityStorage() { release(); }
    MediaFixedCapacityStorage(const MediaFixedCapacityStorage&) = delete;
    MediaFixedCapacityStorage& operator=(const MediaFixedCapacityStorage&) = delete;
    MediaFixedCapacityStorage(MediaFixedCapacityStorage&& other) noexcept { swap(other); }
    MediaFixedCapacityStorage& operator=(MediaFixedCapacityStorage&& other) noexcept
    { if (this != &other) { release(); swap(other); } return *this; }
    void resize(std::size_t size)
    {
        if (m_data || m_capacity) throw std::logic_error("Fixed storage already allocated");
        *this = MediaFixedCapacityStorage(size, *m_resource);
        std::uninitialized_value_construct_n(m_data, size); m_size = size;
    }
    std::size_t size() const noexcept { return m_size; }
    std::size_t capacity() const noexcept { return m_capacity; }
    std::size_t storageBytes() const noexcept { return m_capacity * sizeof(T); }
    std::pmr::memory_resource* resource() const noexcept { return m_resource; }
    bool empty() const noexcept { return m_size == 0; }
    T& operator[](std::size_t index) noexcept { return m_data[position(index)]; }
    const T& operator[](std::size_t index) const noexcept { return m_data[position(index)]; }
    T& front() noexcept { return (*this)[0]; }
    const T& front() const noexcept { return (*this)[0]; }
    T& back() noexcept { return (*this)[m_size - 1]; }
    const T& back() const noexcept { return (*this)[m_size - 1]; }
    void push_back(T value)
    {
        if (m_size == m_capacity) throw std::length_error("Fixed storage exhausted");
        std::construct_at(m_data + position(m_size), std::move(value)); ++m_size;
    }
    void pop_front() noexcept
    { std::destroy_at(&front()); m_head = position(1); --m_size; if (!m_size) m_head = 0; }
    void clear() noexcept { while (!empty()) pop_front(); }
    template<class Predicate> void eraseIf(Predicate predicate)
    {
        const auto last = std::remove_if(begin(), end(), std::move(predicate));
        while (m_size > last.index) { std::destroy_at(&back()); --m_size; }
        if (!m_size) m_head = 0;
    }
    std::span<T> view()
    { if (m_head) throw std::logic_error("Ring storage is not contiguous"); return {m_data, m_size}; }
    std::span<const T> view() const
    { if (m_head) throw std::logic_error("Ring storage is not contiguous"); return {m_data, m_size}; }
    template<bool Const> struct Iterator final {
        using Owner = std::conditional_t<Const, const MediaFixedCapacityStorage, MediaFixedCapacityStorage>;
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const T&, T&>;
        using pointer = std::conditional_t<Const, const T*, T*>;
        Owner* owner; std::size_t index;
        reference operator*() const { return (*owner)[index]; }
        pointer operator->() const { return &**this; }
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        bool operator==(const Iterator&) const = default;
    };
    auto begin() noexcept { return Iterator<false>{this, 0}; }
    auto end() noexcept { return Iterator<false>{this, m_size}; }
    auto begin() const noexcept { return Iterator<true>{this, 0}; }
    auto end() const noexcept { return Iterator<true>{this, m_size}; }
private:
    std::size_t position(std::size_t index) const noexcept
    { return index >= m_capacity - m_head ? index - (m_capacity - m_head) : m_head + index; }
    void release() noexcept
    {
        if (m_data) { clear(); std::pmr::polymorphic_allocator<T>{m_resource}.deallocate(m_data, m_capacity); }
        m_data = nullptr; m_capacity = m_size = m_head = 0;
    }
    void swap(MediaFixedCapacityStorage& other) noexcept
    { std::swap(m_resource, other.m_resource); std::swap(m_data, other.m_data); std::swap(m_capacity, other.m_capacity); std::swap(m_size, other.m_size); std::swap(m_head, other.m_head); }
    std::pmr::memory_resource* m_resource = std::pmr::new_delete_resource();
    T* m_data = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_size = 0;
    std::size_t m_head = 0;
};

template<class T>
::media::Result<MediaFixedCapacityStorage<T>>
MediaFixedCapacityStorage<T>::fromAllocation(MediaOwnedMemoryAllocation allocation)
{
    using Result = ::media::Result<MediaFixedCapacityStorage<T>>;
    if (!allocation || !allocation.bytes() || allocation.bytes() % sizeof(T) ||
        allocation.alignment() != alignof(T))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Fixed storage requires exact element backing and alignment"));
    MediaFixedCapacityStorage<T> storage;
    storage.m_resource = allocation.resource();
    storage.m_capacity = allocation.bytes() / sizeof(T);
    storage.m_data = static_cast<T*>(allocation.release());
    return Result::success(std::move(storage));
}
} // namespace media::ffmpeg::graph
