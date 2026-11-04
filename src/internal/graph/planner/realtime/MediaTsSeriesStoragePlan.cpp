#include "internal/graph/planner/realtime/MediaTsSeriesStoragePlan.h"
#include "internal/graph/protocol/mpegts/MediaTsSeriesStoragePreparation.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaTsSeriesStoragePlan> MediaTsSeriesStoragePlanner::plan(
    std::size_t packets, std::size_t datagrams, std::size_t live, std::size_t views)
{
    using Result = ::media::Result<MediaTsSeriesStoragePlan>;
    if (!live || !views) return Result::failure(::media::ErrorInfo::invalidArgument(
        "TS series storage requires a final ownership bound"));
    auto packetizer = MediaTsTransportPacketizer::seriesStorageGeometry();
    auto protocol = MediaMpegTsProtocolDatagramBatchBuffer::seriesStorageGeometry();
    auto control = MediaTsTransportPacketizer::controlStorageGeometry();
    if (!packetizer || !protocol || !control) return Result::failure(
        !packetizer ? packetizer.error() : !protocol ? protocol.error() : control.error());
    const auto continuity = MediaTsTransportPacketizer::continuityReservationGeometry();
    const auto observation = MediaTsTransportPacketizer::acceptedObservationGeometry();
    const auto ledgers = MediaTsTransportPacketizer::continuityLedgerCount();
    auto geometry = MediaTsSeriesStoragePreparation::geometry(packetizer.value(), protocol.value(), packets, datagrams);
    if (!geometry) return Result::failure(geometry.error());
    auto series = MediaTsSeriesStoragePreparation::storageFootprint(geometry.value());
    if (!series) return Result::failure(series.error());
    constexpr auto limit = (std::numeric_limits<std::size_t>::max)();
    if (!ledgers || !continuity.bytes || !continuity.alignment || live > limit / continuity.bytes ||
        !observation.bytes || !observation.alignment || live > limit / observation.bytes ||
        live > limit / series.value()) return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS series ownership storage is not representable"));
    std::size_t total = live * series.value();
    const auto addAllocation = [&](std::size_t bytes, std::size_t alignment, std::size_t count) -> ::media::Status {
        auto footprint = MediaNetworkMemoryResource::allocationFootprint(bytes, alignment);
        if (!footprint) return ::media::Status::failure(footprint.error());
        if (!count || footprint.value() > (limit - total) / count) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("TS persistent storage footprint overflows"));
        total += footprint.value() * count;
        return ::media::Status::success();
    };
    auto status = addAllocation(control.value().bytes, control.value().alignment, 1);
    if (!status) return Result::failure(status.error());
    status = addAllocation(protocol.value().batchOwner.bytes,
        protocol.value().batchOwner.alignment, views);
    if (!status) return Result::failure(status.error());
    status = addAllocation(live * continuity.bytes, continuity.alignment, ledgers);
    if (!status) return Result::failure(status.error());
    status = addAllocation(live * observation.bytes, observation.alignment, 1);
    if (!status) return Result::failure(status.error());
    // A cached packet owner and its backing retain their leases while idle.
    for (const auto role : {MediaTsSeriesStorageRole::Packets, MediaTsSeriesStorageRole::PacketOwner}) {
        const auto& shape = geometry.value()[static_cast<std::size_t>(role)];
        status = addAllocation(shape.bytes, shape.alignment, 1);
        if (!status) return Result::failure(status.error());
    }
    return Result::success({packets, datagrams, live, views, packetizer.value(), protocol.value(),
        control.value(), continuity, observation, ledgers, total});
}
::media::Status MediaTsSeriesStoragePlanner::validate(const MediaTsSeriesStoragePlan& product)
{
    auto expected = plan(product.maximumPackets, product.maximumDatagrams, product.maximumLiveSeries,
        product.maximumPrefixViews);
    if (!expected) return ::media::Status::failure(expected.error());
    if (expected.value() != product) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "TS series storage differs from its actual allocation geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
