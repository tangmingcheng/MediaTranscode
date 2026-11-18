#include "internal/graph/nodes/output/MediaMpegTsUdpWirePreparationWorkspace.h"
#include <array>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using Shapes = std::array<MediaSharedAllocationGeometry, 2>;
::media::Result<Shapes> shapes(std::size_t entries)
{
    using Result = ::media::Result<Shapes>;
    if (!entries)
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS UDP workspace cardinality is invalid"));
    Shapes result{{
        {entries, sizeof(MediaWireGlobalSequenceReservationEntry), alignof(MediaWireGlobalSequenceReservationEntry)},
        {entries, sizeof(MediaWireDatagramBatchInput), alignof(MediaWireDatagramBatchInput)}}};
    for (auto& shape : result) {
        if (shape.count > (std::numeric_limits<std::size_t>::max)() / shape.bytes)
            return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS UDP workspace array overflows"));
        shape.bytes *= shape.count;
    }
    return Result::success(result);
}
} // namespace

::media::Result<std::size_t> MediaMpegTsUdpWirePreparationWorkspace::storageFootprint(
    std::size_t entries)
{
    using Result = ::media::Result<std::size_t>;
    auto layout = shapes(entries);
    if (!layout) return Result::failure(layout.error());
    std::size_t total = 0;
    for (const auto& shape : layout.value()) {
        auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
        if (!cost) return Result::failure(cost.error());
        if (cost.value() > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS UDP workspace footprint overflows"));
        total += cost.value();
    }
    return Result::success(total);
}

::media::Result<MediaMpegTsUdpWirePreparationWorkspace> MediaMpegTsUdpWirePreparationWorkspace::prepare(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    MediaNetworkStorageLease& lease, std::size_t entries)
{
    using Result = ::media::Result<MediaMpegTsUdpWirePreparationWorkspace>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS UDP workspace has no memory resource"));
    auto layout = shapes(entries);
    if (!layout) return Result::failure(layout.error());
    MediaMpegTsUdpWirePreparationWorkspace result;
    std::size_t index = 0;
    auto allocate = [&]<class T>(MediaFixedCapacityStorage<T>& target) -> ::media::Status {
        const auto& shape = layout.value()[index++];
        auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
        if (!cost) return ::media::Status::failure(cost.error());
        auto portion = lease.split(cost.value());
        if (!portion) return ::media::Status::failure(portion.error());
        auto backing = resource->allocateFromReservation(shape.bytes, shape.alignment, std::move(portion).value());
        if (!backing) return ::media::Status::failure(backing.error());
        auto storage = MediaFixedCapacityStorage<T>::fromAllocation(std::move(backing).value());
        if (!storage) return ::media::Status::failure(storage.error());
        target = std::move(storage).value();
        return ::media::Status::success();
    };
    auto status = allocate(result.globalEntries);
    if (status) status = allocate(result.outputInputs);
    if (!status) return Result::failure(status.error());
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
