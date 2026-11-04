#pragma once
#include "internal/graph/runtime/resource/MediaSharedAllocationState.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include <exception>
#include <limits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
class MediaDatagramCommitTransaction;
struct MediaSharedAllocationAdmitted final {
    std::shared_ptr<MediaNetworkMemoryResource> resource;
    MediaSharedAllocationGeometry geometry;
    // Only synchronous allocate may access this unpublished guard.
    // Stored allocator copies never access it after factory return.
    MediaOwnedMemoryAllocation* prepared;
};
using MediaSharedAllocationScope = std::variant<
    std::shared_ptr<MediaSharedAllocationState>, MediaSharedAllocationAdmitted>;

// Only the audited non-array allocate_shared factories may create this allocator.
// Rebind copies retain the resource through the last weak control-block owner.
// One allocation is proved by the supported STL implementation, not a counter.
template<class T>
class MediaSharedAllocationAllocator final {
public:
    using value_type = T;
    template<class U>
    MediaSharedAllocationAllocator(const MediaSharedAllocationAllocator<U>& other) noexcept
        : m_scope(other.scope()) {}
    T* allocate(std::size_t count)
    {
        if (!count || count > (std::numeric_limits<std::size_t>::max)() / sizeof(T))
            throw MediaSharedAllocationMismatch{};
        const MediaSharedAllocationGeometry requested{count, count * sizeof(T), alignof(T)};
        if (const auto* probe = std::get_if<std::shared_ptr<MediaSharedAllocationState>>(&m_scope)) {
            if (!*probe) throw MediaSharedAllocationMismatch{};
            (*probe)->observe(requested);
        }
        const auto& admitted = std::get<MediaSharedAllocationAdmitted>(m_scope);
        if (!admitted.resource || requested != admitted.geometry)
            throw MediaSharedAllocationMismatch{};
        if (!admitted.prepared || !*admitted.prepared ||
            admitted.prepared->resource() != admitted.resource.get() ||
            admitted.prepared->bytes() != requested.bytes ||
            admitted.prepared->alignment() != requested.alignment)
            throw MediaSharedAllocationMismatch{};
        // Last operation: plain pointer transfer cannot throw. The audited STL
        // guard owns construction failure cleanup after this return.
        return static_cast<T*>(admitted.prepared->release());
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        const auto* admitted = std::get_if<MediaSharedAllocationAdmitted>(&m_scope);
        if (!admitted || !admitted->resource || !count ||
            count > (std::numeric_limits<std::size_t>::max)() / sizeof(T) ||
            MediaSharedAllocationGeometry{count, count * sizeof(T), alignof(T)} != admitted->geometry)
            std::terminate();
        admitted->resource->deallocate(pointer, count * sizeof(T), alignof(T));
    }
    const MediaSharedAllocationScope& scope() const noexcept { return m_scope; }
    template<class U>
    bool operator==(const MediaSharedAllocationAllocator<U>& other) const noexcept
    {
        if (m_scope.index() != other.scope().index()) return false;
        if (const auto* probe = std::get_if<std::shared_ptr<MediaSharedAllocationState>>(&m_scope))
            return *probe == std::get<std::shared_ptr<MediaSharedAllocationState>>(other.scope());
        const auto& left = std::get<MediaSharedAllocationAdmitted>(m_scope);
        const auto& right = std::get<MediaSharedAllocationAdmitted>(other.scope());
        return left.resource == right.resource && left.geometry == right.geometry;
    }
private:
    friend class MediaDatagramCommitTransaction;
    friend class MediaWireDatagramBatchBuffer;
    friend struct MediaTsPacketCursorFactory;
    friend class MediaTsTransportPacketizer;
    friend class MediaMpegTsProtocolDatagramBatchBuffer;
    template<class Data> friend class MediaProtocolOutputGenerationData;
    template<class Object, class... Arguments>
    friend ::media::Result<MediaSharedAllocationGeometry> probeMediaSharedAllocationGeometry();
    explicit MediaSharedAllocationAllocator(MediaSharedAllocationScope scope) noexcept
        : m_scope(std::move(scope)) {}
    MediaSharedAllocationScope m_scope;
};
} // namespace media::ffmpeg::graph
