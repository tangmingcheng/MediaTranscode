#pragma once
#include "internal/graph/nodes/output/MediaRtpWireInputBatch.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"

namespace media::ffmpeg::graph {
// One unpublished AU. All three allocations consume an already admitted lease;
// appending never allocates, grows backing, or reserves additional budget.
class MediaRtpAccessUnitCollection final {
public:
    static ::media::Result<std::size_t> storageFootprint(
        std::size_t datagrams, std::size_t datagramBytes);
    static ::media::Result<MediaRtpAccessUnitCollection> prepare(
        const std::shared_ptr<MediaNetworkMemoryResource>& resource,
        MediaNetworkStorageLease& reservation,
        std::size_t datagrams, std::size_t datagramBytes,
        MediaRunningTime presentation, MediaRunningTime release);
    MediaRtpAccessUnitCollection(MediaRtpAccessUnitCollection&&) noexcept = default;
    MediaRtpAccessUnitCollection& operator=(MediaRtpAccessUnitCollection&&) = delete;
    MediaRtpAccessUnitCollection(const MediaRtpAccessUnitCollection&) = delete;
    MediaRtpAccessUnitCollection& operator=(const MediaRtpAccessUnitCollection&) = delete;

    std::span<std::uint8_t> scratch() noexcept;
    ::media::Status append(std::span<const std::uint8_t> bytes, std::size_t payloadOctets);
    ::media::Status seal(MediaWireMediaBoundary finalBoundary);
    bool sealed() const noexcept { return m_sealed; }
    ::media::Result<std::span<const MediaPacketizedRtpDatagramView>> datagrams() const;

private:
    MediaRtpAccessUnitCollection(MediaOwnedMemoryAllocation bytes,
        MediaFixedCapacityStorage<MediaPacketizedRtpDatagramView> views,
        MediaOwnedMemoryAllocation scratch, std::size_t datagramBytes,
        MediaRunningTime presentation, MediaRunningTime release) noexcept;
    MediaOwnedMemoryAllocation m_bytes;
    MediaFixedCapacityStorage<MediaPacketizedRtpDatagramView> m_views;
    MediaOwnedMemoryAllocation m_scratch;
    std::size_t m_datagramBytes;
    MediaRunningTime m_presentation;
    MediaRunningTime m_release;
    bool m_sealed = false;
};
} // namespace media::ffmpeg::graph
