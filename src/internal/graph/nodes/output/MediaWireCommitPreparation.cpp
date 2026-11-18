#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"
#include <array>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using Geometry = std::array<MediaSharedAllocationGeometry, 5>;
::media::Result<Geometry> geometry(
    const MediaDatagramCommitStoragePlan& plan, std::size_t entries)
{
    using Result = ::media::Result<Geometry>;
    if (!entries || !plan.geometry.count || !plan.geometry.bytes ||
        plan.geometry.bytes % plan.geometry.count ||
        (plan.kind != MediaDatagramCommitStorageKind::Rtp &&
         plan.kind != MediaDatagramCommitStorageKind::MpegTsUdp))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Wire commit preparation requires a complete typed storage plan"));
    if (plan.protocolPrefixGeometry &&
        (plan.protocolPrefixGeometry->count != 1 || !plan.protocolPrefixGeometry->bytes ||
         !plan.protocolPrefixGeometry->alignment ||
         (plan.protocolPrefixGeometry->alignment & (plan.protocolPrefixGeometry->alignment - 1))))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Wire commit protocol prefix geometry is invalid"));
    const std::size_t actionSize = plan.kind == MediaDatagramCommitStorageKind::Rtp
        ? sizeof(MediaRtpWireCommitAction) : 0;
    for (const auto size : {sizeof(std::uint64_t), sizeof(MediaRunningTime), actionSize}) {
        if (size && entries > (std::numeric_limits<std::size_t>::max)() / size)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Wire commit preparation array size overflows"));
    }
    return Result::success(Geometry{{plan.geometry,
        {entries, entries * sizeof(std::uint64_t), alignof(std::uint64_t)},
        {entries, entries * sizeof(MediaRunningTime), alignof(MediaRunningTime)},
        {entries, entries * actionSize, alignof(MediaRtpWireCommitAction)},
        plan.protocolPrefixGeometry.value_or(MediaSharedAllocationGeometry{0, 0, 0})}});
}
::media::Result<std::array<std::size_t, 5>> footprints(const Geometry& geometry)
{
    using Result = ::media::Result<std::array<std::size_t, 5>>;
    std::array<std::size_t, 5> costs{};
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        if (!geometry[index].bytes) continue; // Absent protocol roles have no corresponding allocation.
        auto cost = MediaNetworkMemoryResource::allocationFootprint(
            geometry[index].bytes, geometry[index].alignment);
        if (!cost) return Result::failure(cost.error());
        costs[index] = cost.value();
    }
    return Result::success(costs);
}
::media::Result<std::size_t> totalFootprint(const std::array<std::size_t, 5>& costs)
{
    using Result = ::media::Result<std::size_t>;
    std::size_t total = 0;
    for (const auto cost : costs) {
        if (cost > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Wire commit preparation footprint overflows"));
        total += cost;
    }
    return Result::success(total);
}
} // namespace

::media::Result<std::size_t> MediaWireCommitPreparation::storageFootprint(
    const MediaDatagramCommitStoragePlan& plan, std::size_t entries)
{
    auto shape = geometry(plan, entries);
    if (!shape) return ::media::Result<std::size_t>::failure(shape.error());
    auto costs = footprints(shape.value());
    if (!costs) return ::media::Result<std::size_t>::failure(costs.error());
    return totalFootprint(costs.value());
}

::media::Result<MediaNetworkStorageLease> MediaWireCommitPreparation::admit(
    const std::shared_ptr<const MediaWireGlobalSequenceState>& identity,
    std::size_t entries, std::size_t outputBytes, std::size_t workspaceBytes)
{
    using Result = ::media::Result<MediaNetworkStorageLease>;
    if (!identity || !identity->storageResource() || !outputBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire admission requires complete owners and output size"));
    auto commitBytes = storageFootprint(identity->commitStorage(), entries);
    if (!commitBytes) return Result::failure(commitBytes.error());
    std::size_t total = commitBytes.value();
    for (const auto bytes : {outputBytes, workspaceBytes}) {
        if (bytes > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument("Wire complete preparation footprint overflows"));
        total += bytes;
    }
    return identity->storageResource()->reserveStorage(total, MediaStorageAdmission::Backpressure);
}

::media::Result<MediaWireCommitPreparation> MediaWireCommitPreparation::prepare(
    std::shared_ptr<const MediaWireGlobalSequenceState> identity,
    std::span<const MediaWireDatagramBatchInput> inputs,
    const MediaDatagramBatchPlan& batchPlan)
{
    using Result = ::media::Result<MediaWireCommitPreparation>;
    if (!identity) return Result::failure(::media::ErrorInfo::invalidArgument("Wire preparation requires its identity"));
    auto output = MediaWireDatagramBatchPartitionBuilder::storageFootprint(
        inputs, batchPlan, identity->commitStorage().batchGeometry);
    if (!output) return Result::failure(output.error());
    auto lease = admit(identity, inputs.size(), output.value(), 0);
    if (!lease) return Result::failure(lease.error());
    return fromReservation(std::move(identity), inputs, batchPlan, std::move(lease).value());
}

::media::Result<MediaWireCommitPreparation> MediaWireCommitPreparation::fromReservation(
    std::shared_ptr<const MediaWireGlobalSequenceState> identity,
    std::span<const MediaWireDatagramBatchInput> inputs,
    const MediaDatagramBatchPlan& batchPlan, MediaNetworkStorageLease lease)
{
    using Result = ::media::Result<MediaWireCommitPreparation>;
    if (!identity || !identity->storageResource()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Wire commit preparation requires its planned memory resource"));
    const auto& plan = identity->commitStorage();
    const auto& resource = identity->storageResource();
    auto shape = geometry(plan, inputs.size());
    if (!shape) return Result::failure(shape.error());
    auto costs = footprints(shape.value());
    if (!costs) return Result::failure(costs.error());
    auto total = totalFootprint(costs.value());
    if (!total) return Result::failure(total.error());
    auto outputCost = MediaWireDatagramBatchPartitionBuilder::storageFootprint(inputs, batchPlan, plan.batchGeometry);
    if (!outputCost) return Result::failure(outputCost.error());
    if (outputCost.value() > (std::numeric_limits<std::size_t>::max)() - total.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire complete output footprint overflows"));
    if (lease.bytes() != total.value() + outputCost.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire preparation reservation differs from exact storage"));
    std::array<std::optional<MediaOwnedMemoryAllocation>, 5> blocks;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        if (!costs.value()[index]) continue;
        auto portion = lease.split(costs.value()[index]);
        if (!portion) return Result::failure(portion.error());
        auto block = resource->allocateFromReservation(shape.value()[index].bytes,
            shape.value()[index].alignment, std::move(portion).value());
        if (!block) return Result::failure(block.error());
        blocks[index].emplace(std::move(block).value());
    }
    auto wire = MediaFixedCapacityStorage<std::uint64_t>::fromAllocation(std::move(*blocks[1]));
    if (!wire) return Result::failure(wire.error());
    auto times = MediaFixedCapacityStorage<MediaRunningTime>::fromAllocation(std::move(*blocks[2]));
    if (!times) return Result::failure(times.error());
    auto output = MediaWireDatagramBatchPartitionBuilder::prepare(identity, inputs, batchPlan,
        plan.batchGeometry, resource, std::move(lease));
    if (!output) return Result::failure(output.error());
    return Result::success(MediaWireCommitPreparation{std::move(*blocks[0]),
        std::move(blocks[3]), std::move(blocks[4]), {std::move(wire).value(), std::move(times).value()}, std::move(output).value()});
}
} // namespace media::ffmpeg::graph
