#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/runtime/storage/MediaOwnedByteStorage.h"
#include <algorithm>
#include <span>

namespace media::ffmpeg::graph {

// Accepted CNAME bytes own exactly their wire length; no spare capacity,
// terminator, iterator proxy or allocator-dependent growth.
class MediaRtcpBytes final {
public:
    using Allocator = MediaLineageAllocator<std::uint8_t>;
    explicit MediaRtcpBytes(Allocator allocator)
        : m_allocator(std::move(allocator)), m_bytes(*m_allocator.resource(), 0) {}
    MediaRtcpBytes(std::span<const std::uint8_t> bytes, Allocator allocator)
        : m_allocator(std::move(allocator)), m_bytes(*m_allocator.resource(), bytes) {}
    MediaRtcpBytes(const MediaRtcpBytes& other) : MediaRtcpBytes(other.m_bytes.view(), other.m_allocator) {}
    MediaRtcpBytes(MediaRtcpBytes&&) noexcept = default;
    MediaRtcpBytes& operator=(MediaRtcpBytes&&) noexcept = default;
    MediaRtcpBytes& operator=(const MediaRtcpBytes& other)
    {
        if (this != &other) { MediaRtcpBytes replacement(other); *this = std::move(replacement); }
        return *this;
    }
    const Allocator& get_allocator() const noexcept { return m_allocator; }
    std::size_t size() const noexcept { return m_bytes.size(); }
    bool empty() const noexcept { return m_bytes.empty(); }
    void clear() noexcept { m_bytes = MediaOwnedByteStorage(*m_allocator.resource(), 0); }
    bool matches(std::span<const std::uint8_t> value) const noexcept
    { return std::ranges::equal(m_bytes.view(), value); }
    friend bool operator==(const MediaRtcpBytes& left, const MediaRtcpBytes& right) noexcept
    { return left.matches(right.m_bytes.view()); }
private:
    Allocator m_allocator;
    MediaOwnedByteStorage m_bytes;
};

} // namespace media::ffmpeg::graph
