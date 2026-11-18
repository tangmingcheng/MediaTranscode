#pragma once
#include "internal/graph/planner/realtime/MediaTsSeriesStorageGeometry.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include <array>
#include <optional>

namespace media::ffmpeg::graph {
// Allocation roles for a complete series. The wire prefix has a separate
// preparation/admission and is deliberately absent here.
enum class MediaTsSeriesStorageRole : std::size_t {
    Packets, PacketOwner, Cursor, Descriptors, BatchOwner, CommitOwner, CommitModel, Count
};
struct MediaTsSeriesStoragePreparation final {
    using Geometry = std::array<MediaSharedAllocationGeometry,
        static_cast<std::size_t>(MediaTsSeriesStorageRole::Count)>;
    std::array<std::optional<MediaOwnedMemoryAllocation>,
        static_cast<std::size_t>(MediaTsSeriesStorageRole::Count)> blocks;
    static ::media::Result<Geometry> geometry(
        const MediaTsPacketizerSeriesStorageGeometry& packetizer,
        const MediaTsProtocolSeriesStorageGeometry& protocol,
        std::size_t packets, std::size_t datagrams);
    static ::media::Result<std::size_t> storageFootprint(const Geometry& geometry);
    static ::media::Result<MediaTsSeriesStoragePreparation> prepare(
        const std::shared_ptr<MediaNetworkMemoryResource>& resource, const Geometry& geometry,
        bool reuseExactPacketStorage);
    static ::media::Result<MediaTsSeriesStoragePreparation> fromReservation(
        const std::shared_ptr<MediaNetworkMemoryResource>& resource, const Geometry& geometry,
        MediaNetworkStorageLease lease, bool reuseExactPacketStorage);
    MediaOwnedMemoryAllocation take(MediaTsSeriesStorageRole role) noexcept
    {
        auto& block = blocks[static_cast<std::size_t>(role)];
        auto allocation = std::move(*block);
        block.reset();
        return allocation;
    }
};
} // namespace media::ffmpeg::graph
