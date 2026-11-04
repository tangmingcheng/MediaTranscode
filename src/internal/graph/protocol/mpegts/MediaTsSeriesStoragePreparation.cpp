#include "internal/graph/protocol/mpegts/MediaTsSeriesStoragePreparation.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaTsSeriesStoragePreparation::Geometry>
MediaTsSeriesStoragePreparation::geometry(
    const MediaTsPacketizerSeriesStorageGeometry& packetizer,
    const MediaTsProtocolSeriesStorageGeometry& protocol,
    std::size_t packets, std::size_t datagrams)
{
    using Result = ::media::Result<Geometry>;
    using Packet = std::array<std::uint8_t, 188>;
    if (!packets || !datagrams || datagrams > packets ||
        packets > (std::numeric_limits<std::size_t>::max)() / sizeof(Packet) ||
        datagrams > (std::numeric_limits<std::size_t>::max)() / sizeof(MediaMpegTsProtocolDatagram))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS series preparation requires representable packet and descriptor counts"));
    Geometry result{{
        {packets, packets * sizeof(Packet), alignof(Packet)},
        packetizer.packetOwner, packetizer.cursor,
        {datagrams, datagrams * sizeof(MediaMpegTsProtocolDatagram), alignof(MediaMpegTsProtocolDatagram)},
        protocol.batchOwner, protocol.commitOwner, protocol.commitModel}};
    auto valid = storageFootprint(result);
    if (!valid) return Result::failure(valid.error());
    return Result::success(result);
}

::media::Result<std::size_t> MediaTsSeriesStoragePreparation::storageFootprint(
    const Geometry& geometry)
{
    using Result = ::media::Result<std::size_t>;
    std::size_t total = 0;
    for (const auto& shape : geometry) {
        if (!shape.count || !shape.bytes || shape.bytes % shape.count ||
            !shape.alignment || (shape.alignment & (shape.alignment - 1)))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "TS series preparation geometry is incomplete"));
        auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
        if (!cost) return Result::failure(cost.error());
        if (cost.value() > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "TS series preparation footprint overflows"));
        total += cost.value();
    }
    return Result::success(total);
}

::media::Result<MediaTsSeriesStoragePreparation> MediaTsSeriesStoragePreparation::prepare(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource, const Geometry& geometry,
    bool reuseExactPacketStorage)
{
    using Result = ::media::Result<MediaTsSeriesStoragePreparation>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument(
        "TS series preparation requires its generation memory resource"));
    auto total = storageFootprint(geometry);
    if (!total) return Result::failure(total.error());
    // Admission precedes every allocation. No cursor/revision/continuity state
    // is owned or changed here, and partial physical allocation unwinds by RAII.
    std::size_t required = total.value();
    if (reuseExactPacketStorage) {
        for (auto role : {MediaTsSeriesStorageRole::Packets, MediaTsSeriesStorageRole::PacketOwner}) {
            const auto& shape = geometry[static_cast<std::size_t>(role)];
            auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
            if (!cost) return Result::failure(cost.error());
            required -= cost.value();
        }
    }
    auto admitted = resource->reserveStorage(required, MediaStorageAdmission::Backpressure);
    if (!admitted) return Result::failure(admitted.error());
    return fromReservation(resource, geometry, std::move(admitted).value(), reuseExactPacketStorage);
}

::media::Result<MediaTsSeriesStoragePreparation> MediaTsSeriesStoragePreparation::fromReservation(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource, const Geometry& geometry,
    MediaNetworkStorageLease lease, bool reuseExactPacketStorage)
{
    using Result = ::media::Result<MediaTsSeriesStoragePreparation>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("TS series resource is absent"));
    auto total = storageFootprint(geometry);
    if (!total) return Result::failure(total.error());
    std::size_t required = total.value();
    if (reuseExactPacketStorage) {
        for (auto role : {MediaTsSeriesStorageRole::Packets, MediaTsSeriesStorageRole::PacketOwner}) {
            const auto& shape = geometry[static_cast<std::size_t>(role)];
            auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
            if (!cost) return Result::failure(cost.error());
            required -= cost.value();
        }
    }
    if (lease.bytes() != required) return Result::failure(::media::ErrorInfo::invalidArgument(
        "TS series reservation differs from its prepared allocation geometry"));
    MediaTsSeriesStoragePreparation prepared;
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        if (reuseExactPacketStorage &&
            (index == static_cast<std::size_t>(MediaTsSeriesStorageRole::Packets) ||
             index == static_cast<std::size_t>(MediaTsSeriesStorageRole::PacketOwner))) continue;
        const auto& shape = geometry[index];
        auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
        if (!cost) return Result::failure(cost.error());
        auto portion = lease.split(cost.value());
        if (!portion) return Result::failure(portion.error());
        auto allocation = resource->allocateFromReservation(shape.bytes, shape.alignment,
            std::move(portion).value());
        if (!allocation) return Result::failure(allocation.error());
        prepared.blocks[index].emplace(std::move(allocation).value());
    }
    return Result::success(std::move(prepared));
}
} // namespace media::ffmpeg::graph
