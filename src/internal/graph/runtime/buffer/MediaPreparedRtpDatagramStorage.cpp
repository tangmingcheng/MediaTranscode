#include "internal/graph/runtime/buffer/MediaPreparedRtpDatagramStorage.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

// allocate_shared rebinds this exact allocator to its implementation-owned type.
// Read back that request instead of predicting shared_ptr control-block layout.
template<class T>
struct EntryAllocator {
    using value_type = T;
    std::pmr::memory_resource* resource;
    MediaPreparedRtpDatagramAllocationFacts expected;

    EntryAllocator(std::pmr::memory_resource& storage,
        MediaPreparedRtpDatagramAllocationFacts facts) noexcept
        : resource(&storage), expected(facts) {}
    template<class U>
    EntryAllocator(const EntryAllocator<U>& other) noexcept
        : resource(other.resource), expected(other.expected) {}

    T* allocate(std::size_t count)
    {
        if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(T))
            throw std::bad_alloc();
        const auto bytes = count * sizeof(T);
        if (expected.bytes && (expected.bytes != bytes || expected.alignment != alignof(T)))
            throw std::bad_alloc();
        return static_cast<T*>(resource->allocate(bytes, alignof(T)));
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    { resource->deallocate(pointer, count * sizeof(T), alignof(T)); }
    template<class U>
    bool operator==(const EntryAllocator<U>& other) const noexcept
    { return resource->is_equal(*other.resource); }
};

class EntryAllocationReadback final : public std::pmr::memory_resource {
public:
    MediaPreparedRtpDatagramAllocationFacts facts{0, 0};
private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        // This adapter supports the single-allocation implementation confirmed
        // by this readback. Other library implementations fail before capture.
        if (facts.bytes || !bytes) throw std::bad_alloc();
        auto* pointer = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        facts = {bytes, alignment};
        return pointer;
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
    { std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment); }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    { return this == &other; }
};

} // namespace

::media::Result<MediaPreparedRtpDatagramAllocationFacts>
MediaPreparedRtpDatagramStorage::allocationFacts()
{
    using Result = ::media::Result<MediaPreparedRtpDatagramAllocationFacts>;
    try {
        EntryAllocationReadback resource;
        auto owner = std::allocate_shared<MediaPreparedRawRtpDatagram>(
            EntryAllocator<MediaPreparedRawRtpDatagram>{resource, {0, 0}},
            MediaPreparedRawRtpDatagram{
                MediaRtpUdpDatagram{MediaRtpUdpChannel::Rtp, MediaOwnedByteStorage{resource, 0}},
                0, 0});
        if (!resource.facts.bytes || !resource.facts.alignment)
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "Prepared RTP entry allocation readback produced no storage facts"));
        return Result::success(resource.facts);
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Prepared RTP entry requires a verified single allocation"));
    }
}

MediaPreparedRtpDatagramOwner MediaPreparedRtpDatagramStorage::create(
    std::pmr::memory_resource& resource,
    const MediaPreparedRtpDatagramAllocationFacts& allocation,
    MediaRtpUdpDatagram datagram, std::int64_t observedAtNs, std::uint64_t ordinal)
{
    if (!allocation.bytes || !allocation.alignment) throw std::bad_alloc();
    return std::allocate_shared<MediaPreparedRawRtpDatagram>(
        EntryAllocator<MediaPreparedRawRtpDatagram>{resource, allocation},
        MediaPreparedRawRtpDatagram{std::move(datagram), observedAtNs, ordinal});
}

} // namespace media::ffmpeg::graph
