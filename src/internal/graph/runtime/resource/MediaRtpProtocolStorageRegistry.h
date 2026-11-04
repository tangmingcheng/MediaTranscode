#pragma once

#include "internal/graph/planner/realtime/MediaRtpProtocolStoragePlan.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include <memory>
#include <mutex>
#include <span>

namespace media::ffmpeg::graph {
class MediaRtpProtocolStorageRegistry;
class MediaRtpProtocolStoragePermit final {
public:
    MediaRtpProtocolStoragePermit(MediaRtpProtocolStoragePermit&& other) noexcept;
    MediaRtpProtocolStoragePermit& operator=(MediaRtpProtocolStoragePermit&&) = delete;
    MediaRtpProtocolStoragePermit(const MediaRtpProtocolStoragePermit&) = delete;
    MediaRtpProtocolStoragePermit& operator=(const MediaRtpProtocolStoragePermit&) = delete;
    ~MediaRtpProtocolStoragePermit();
    std::size_t capacity() const noexcept { return m_capacity; }
    const MediaSharedAllocationGeometry& allocationGeometry() const noexcept;
    std::pmr::memory_resource& resource() const noexcept;
private:
    friend class MediaRtpProtocolStorageRegistry;
    MediaRtpProtocolStoragePermit(std::shared_ptr<MediaRtpProtocolStorageRegistry> owner,
        std::size_t index, std::size_t capacity) noexcept;
    std::shared_ptr<MediaRtpProtocolStorageRegistry> m_owner;
    std::size_t m_index;
    std::size_t m_capacity;
};

// A fixed activation-time table. It never calls protocol/global state.
class MediaRtpProtocolStorageRegistry final
    : public std::enable_shared_from_this<MediaRtpProtocolStorageRegistry> {
public:
    static ::media::Result<std::shared_ptr<MediaRtpProtocolStorageRegistry>> create(
        std::span<const MediaRtpProtocolStoragePlan> plans,
        std::shared_ptr<MediaNetworkMemoryResource> resource);
    static ::media::Result<std::size_t> storageFootprint(std::uint64_t instances);
private:
    friend class MediaWireGlobalSequenceState;
    ::media::Result<MediaRtpProtocolStoragePermit> claim(
        std::uint64_t rtpEndpointId, std::uint64_t rtcpEndpointId,
        std::size_t capacity, std::shared_ptr<MediaNodeWakeup> wakeup);
public:
    void notify() noexcept;
private:
    friend class MediaRtpProtocolStoragePermit;
    struct Entry final {
        Entry(MediaRtpProtocolStoragePlan plan, MediaOwnedMemoryAllocation backing) noexcept;
        Entry(Entry&& other) noexcept;
        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
        Entry& operator=(Entry&&) = delete;
        MediaRtpProtocolStoragePlan plan;
        bool claimed = false;
        std::weak_ptr<MediaNodeWakeup> wakeup;
        // Destruction order: the resource releases before its original backing.
        MediaOwnedMemoryAllocation backing;
        std::pmr::monotonic_buffer_resource recordResource;
    };
    MediaRtpProtocolStorageRegistry(std::span<const MediaRtpProtocolStoragePlan> plans,
        std::shared_ptr<MediaNetworkMemoryResource> resource);
    void release(std::size_t index) noexcept;
    const std::shared_ptr<MediaNetworkMemoryResource> m_resource;
    std::mutex m_mutex;
    MediaFixedCapacityStorage<Entry> m_entries;
};
} // namespace media::ffmpeg::graph
