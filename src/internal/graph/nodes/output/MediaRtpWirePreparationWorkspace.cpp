#include "internal/graph/nodes/output/MediaRtpWirePreparationWorkspace.h"
#include <array>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using Shapes = std::array<MediaSharedAllocationGeometry, 6>;
::media::Result<Shapes> shapes(std::size_t media, std::size_t entries)
{
    using Result = ::media::Result<Shapes>;
    if (!media || entries < media || entries - media > 1)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP workspace cardinality is invalid"));
    Shapes result{{
        {media, sizeof(MediaRtpWirePacketPlan), alignof(MediaRtpWirePacketPlan)},
        {media, sizeof(MediaRtpTimestamp), alignof(MediaRtpTimestamp)},
        {media, sizeof(std::uint64_t), alignof(std::uint64_t)},
        {media, sizeof(MediaRunningTime), alignof(MediaRunningTime)},
        {entries, sizeof(MediaWireGlobalSequenceReservationEntry), alignof(MediaWireGlobalSequenceReservationEntry)},
        {entries, sizeof(MediaWireDatagramBatchInput), alignof(MediaWireDatagramBatchInput)}}};
    for (auto& shape : result) {
        if (shape.count > (std::numeric_limits<std::size_t>::max)() / shape.bytes)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP workspace array overflows"));
        shape.bytes *= shape.count;
    }
    return Result::success(result);
}
} // namespace

::media::Result<std::size_t> MediaRtpWirePreparationWorkspace::storageFootprint(
    std::size_t mediaDatagrams, std::size_t entries)
{
    using Result = ::media::Result<std::size_t>;
    auto layout = shapes(mediaDatagrams, entries);
    if (!layout) return Result::failure(layout.error());
    std::size_t total = 0;
    for (const auto& shape : layout.value()) {
        auto cost = MediaNetworkMemoryResource::allocationFootprint(shape.bytes, shape.alignment);
        if (!cost) return Result::failure(cost.error());
        if (cost.value() > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP workspace footprint overflows"));
        total += cost.value();
    }
    return Result::success(total);
}

::media::Result<MediaRtpWirePreparationWorkspace> MediaRtpWirePreparationWorkspace::prepare(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    MediaNetworkStorageLease& lease, std::size_t mediaDatagrams, std::size_t entries)
{
    using Result = ::media::Result<MediaRtpWirePreparationWorkspace>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("RTP workspace has no memory resource"));
    auto layout = shapes(mediaDatagrams, entries);
    if (!layout) return Result::failure(layout.error());
    MediaRtpWirePreparationWorkspace result;
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
    auto status = allocate(result.plans);
    if (status) status = allocate(result.timestamps);
    if (status) status = allocate(result.payloadOctets);
    if (status) status = allocate(result.deadlines);
    if (status) status = allocate(result.globalEntries);
    if (status) status = allocate(result.outputInputs);
    if (!status) return Result::failure(status.error());
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
