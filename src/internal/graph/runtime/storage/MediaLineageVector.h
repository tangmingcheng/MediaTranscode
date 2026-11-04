#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include <vector>
#include <utility>

namespace media::ffmpeg::graph {

// Owning protocol arrays preserve their allocation domain through copies. Use the
// allocator-extended move: debug iterator proxy allocation is fallible, unlike
// the ordinary noexcept vector move. No underlying vector escapes this owner.
template<class T>
class MediaLineageVector final {
public:
    using Allocator = MediaLineageAllocator<T>;
    static_assert(!std::allocator_traits<Allocator>::is_always_equal::value);
    explicit MediaLineageVector(Allocator allocator) : m_values(allocator) {}
    template<class Iterator>
    MediaLineageVector(Iterator first, Iterator last, Allocator allocator)
        : m_values(first, last, allocator) {}
    MediaLineageVector(const MediaLineageVector& other)
        : m_values(other.m_values, other.get_allocator()) {}
    MediaLineageVector(MediaLineageVector&& other)
        : m_values(std::move(other.m_values), other.get_allocator()) {}
    MediaLineageVector& operator=(const MediaLineageVector& other)
    {
        if (this != &other) { MediaLineageVector replacement(other); m_values.swap(replacement.m_values); }
        return *this;
    }
    MediaLineageVector& operator=(MediaLineageVector&& other)
    {
        if (this != &other) { MediaLineageVector replacement(std::move(other)); m_values.swap(replacement.m_values); }
        return *this;
    }
    Allocator get_allocator() const noexcept { return m_values.get_allocator(); }
    auto begin() noexcept { return m_values.begin(); }
    auto end() noexcept { return m_values.end(); }
    auto begin() const noexcept { return m_values.begin(); }
    auto end() const noexcept { return m_values.end(); }
    const T& front() const noexcept { return m_values.front(); }
    T* data() noexcept { return m_values.data(); }
    const T* data() const noexcept { return m_values.data(); }
    T& back() noexcept { return m_values.back(); }
    const T& back() const noexcept { return m_values.back(); }
    T& operator[](std::size_t index) noexcept { return m_values[index]; }
    const T& operator[](std::size_t index) const noexcept { return m_values[index]; }
    template<class... Arguments> T& emplace_back(Arguments&&... arguments)
    { return m_values.emplace_back(std::forward<Arguments>(arguments)...); }
    std::size_t size() const noexcept { return m_values.size(); }
    bool empty() const noexcept { return m_values.empty(); }
    void clear() noexcept { m_values.clear(); }
    void reserve(std::size_t count) { m_values.reserve(count); }
    template<class Value> void push_back(Value&& value)
    { m_values.push_back(std::forward<Value>(value)); }
    template<class Iterator> void assign(Iterator first, Iterator last)
    { m_values.assign(first, last); }
    friend bool operator==(const MediaLineageVector& left, const MediaLineageVector& right)
    { return left.m_values == right.m_values; }
private:
    std::vector<T, Allocator> m_values;
};

} // namespace media::ffmpeg::graph
