#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include <limits>

namespace media::ffmpeg::graph {
namespace {
// The sizing and construction passes use exactly the same partition cursor.
template<class Visitor>
::media::Status visitPartitions(std::span<const MediaWireDatagramBatchInput> inputs,
    const MediaDatagramBatchPlan& plan, Visitor&& visitor)
{
    if (inputs.empty()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Wire output preparation requires datagrams"));
    MediaWireDatagramPartitionLayout layout;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto& input = inputs[index];
        auto next = layout.next(plan, input.bytes.size(), input.canonicalDeadline);
        if (!next) return ::media::Status::failure(next.error());
        if (layout.datagrams && next.value().datagrams == 1) {
            auto visited = visitor(inputs.subspan(begin, index - begin), layout);
            if (!visited) return visited;
            begin = index;
        }
        layout = next.value();
    }
    return visitor(inputs.subspan(begin), layout);
}

::media::Result<std::size_t> arrayBytes(std::size_t count, std::size_t element)
{
    if (!count || count > (std::numeric_limits<std::size_t>::max)() / element)
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("Wire output backing size overflows"));
    return ::media::Result<std::size_t>::success(count * element);
}
::media::Status addCost(std::size_t& total, std::size_t bytes, std::size_t alignment)
{
    auto cost = MediaNetworkMemoryResource::allocationFootprint(bytes, alignment);
    if (!cost) return ::media::Status::failure(cost.error());
    if (cost.value() > (std::numeric_limits<std::size_t>::max)() - total)
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Wire output footprint overflows"));
    total += cost.value();
    return ::media::Status::success();
}
::media::Result<MediaOwnedMemoryAllocation> takeBacking(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    MediaNetworkStorageLease& lease, std::size_t bytes, std::size_t alignment)
{
    using Result = ::media::Result<MediaOwnedMemoryAllocation>;
    auto cost = MediaNetworkMemoryResource::allocationFootprint(bytes, alignment);
    if (!cost) return Result::failure(cost.error());
    auto portion = lease.split(cost.value());
    if (!portion) return Result::failure(portion.error());
    return resource->allocateFromReservation(bytes, alignment, std::move(portion).value());
}
template<class T>
::media::Result<MediaFixedCapacityStorage<T>> takeArray(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    MediaNetworkStorageLease& lease, std::size_t count)
{
    using Result = ::media::Result<MediaFixedCapacityStorage<T>>;
    auto bytes = arrayBytes(count, sizeof(T));
    if (!bytes) return Result::failure(bytes.error());
    auto backing = takeBacking(resource, lease, bytes.value(), alignof(T));
    if (!backing) return Result::failure(backing.error());
    return MediaFixedCapacityStorage<T>::fromAllocation(std::move(backing).value());
}
} // namespace

::media::Result<std::size_t> MediaWireDatagramBatchPartitionBuilder::storageOverheadContribution(
    std::size_t datagrams, const MediaSharedAllocationGeometry& geometry)
{
    using Result = ::media::Result<std::size_t>;
    if (!datagrams || !geometry.count || !geometry.bytes || !geometry.alignment)
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire output contribution requires complete geometry"));
    // One datagram per partition is the maximum fragmentation. Payload bytes
    // have their own ledger contribution; only their allocation header is here.
    auto published = partitionStorageFootprint(1, 1, geometry);
    if (!published) return Result::failure(published.error());
    std::size_t unit = published.value();
    for (const auto& shape : {MediaSharedAllocationGeometry{1, sizeof(Partition), alignof(Partition)},
            MediaSharedAllocationGeometry{1, sizeof(std::shared_ptr<MediaWireDatagramBatchBuffer>),
                alignof(std::shared_ptr<MediaWireDatagramBatchBuffer>)}}) {
        auto status = addCost(unit, shape.bytes, shape.alignment);
        if (!status) return Result::failure(status.error());
    }
    return arrayBytes(datagrams, unit - 1);
}

::media::Result<std::size_t> MediaWireDatagramBatchPartitionBuilder::partitionStorageFootprint(
    std::size_t datagrams, std::size_t payloadBytes, const MediaSharedAllocationGeometry& geometry)
{
    using Result = ::media::Result<std::size_t>;
    if (!datagrams || !payloadBytes || !geometry.count || !geometry.bytes || !geometry.alignment)
        return Result::failure(::media::ErrorInfo::invalidArgument("Published partition requires complete geometry"));
    auto descriptors = arrayBytes(datagrams, sizeof(MediaWireDatagram));
    if (!descriptors) return Result::failure(descriptors.error());
    std::size_t total = 0;
    auto status = addCost(total, payloadBytes, alignof(std::uint8_t));
    if (status) status = addCost(total, descriptors.value(), alignof(MediaWireDatagram));
    if (status) status = addCost(total, geometry.bytes, geometry.alignment);
    return status ? Result::success(total) : Result::failure(status.error());
}

::media::Result<std::size_t> MediaWireDatagramBatchPartitionBuilder::publishedStorageUpperBound(
    std::size_t datagrams, std::size_t payloadBytes, const MediaSharedAllocationGeometry& geometry)
{
    using Result = ::media::Result<std::size_t>;
    if (payloadBytes < datagrams) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Published datagrams require positive individual payloads"));
    auto single = partitionStorageFootprint(1, 1, geometry);
    if (!single) return Result::failure(single.error());
    auto overhead = arrayBytes(datagrams, single.value() - 1);
    if (!overhead) return Result::failure(overhead.error());
    if (payloadBytes > (std::numeric_limits<std::size_t>::max)() - overhead.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Published storage bound overflows"));
    return Result::success(payloadBytes + overhead.value());
}

::media::Result<std::size_t> MediaWireDatagramBatchPartitionBuilder::publicationCollectionStorageFootprint(
    std::size_t partitions)
{
    auto bytes = arrayBytes(partitions, sizeof(std::shared_ptr<MediaWireDatagramBatchBuffer>));
    if (!bytes) return ::media::Result<std::size_t>::failure(bytes.error());
    return MediaNetworkMemoryResource::allocationFootprint(bytes.value(),
        alignof(std::shared_ptr<MediaWireDatagramBatchBuffer>));
}

::media::Status MediaWireDatagramBatchPartitionBuilder::Sizing::accountPartition(
    const MediaWireDatagramPartitionLayout& layout)
{
    if (!m_geometry.count || !m_geometry.bytes || !m_geometry.alignment ||
        !layout.datagrams || layout.datagrams > (std::numeric_limits<std::size_t>::max)() ||
        layout.bytes > (std::numeric_limits<std::size_t>::max)() ||
        m_partitions == (std::numeric_limits<std::size_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Wire batch sizing requires representable layout and geometry"));
    auto cost = partitionStorageFootprint(static_cast<std::size_t>(layout.datagrams),
        static_cast<std::size_t>(layout.bytes), m_geometry);
    if (!cost) return ::media::Status::failure(cost.error());
    if (cost.value() > (std::numeric_limits<std::size_t>::max)() - m_total)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Partition total footprint overflows"));
    m_total += cost.value();
    ++m_partitions;
    return ::media::Status::success();
}

::media::Status MediaWireDatagramBatchPartitionBuilder::Sizing::append(
    std::size_t bytes, MediaRunningTime deadline)
{
    auto next = m_layout.next(m_plan, bytes, deadline);
    if (!next) return ::media::Status::failure(next.error());
    if (m_layout.datagrams && next.value().datagrams == 1) {
        auto accounted = accountPartition(m_layout);
        if (!accounted) return accounted;
    }
    m_layout = next.value();
    return ::media::Status::success();
}

::media::Result<std::size_t>
MediaWireDatagramBatchPartitionBuilder::Sizing::storageFootprint() const
{
    using Result = ::media::Result<std::size_t>;
    auto completed = *this;
    auto accounted = completed.accountPartition(m_layout);
    if (!accounted) return Result::failure(accounted.error());
    auto partitions = arrayBytes(completed.m_partitions, sizeof(Partition));
    auto output = arrayBytes(completed.m_partitions, sizeof(std::shared_ptr<MediaWireDatagramBatchBuffer>));
    if (!partitions || !output) return Result::failure(!partitions ? partitions.error() : output.error());
    auto status = addCost(completed.m_total, partitions.value(), alignof(Partition));
    if (!status) return Result::failure(status.error());
    status = addCost(completed.m_total, output.value(), alignof(std::shared_ptr<MediaWireDatagramBatchBuffer>));
    if (!status) return Result::failure(status.error());
    return Result::success(completed.m_total);
}

::media::Result<std::size_t> MediaWireDatagramBatchPartitionBuilder::storageFootprint(
    std::span<const MediaWireDatagramBatchInput> inputs, const MediaDatagramBatchPlan& plan,
    const MediaSharedAllocationGeometry& geometry)
{
    Sizing sizing(plan, geometry);
    for (const auto& input : inputs) {
        auto appended = sizing.append(input.bytes.size(), input.canonicalDeadline);
        if (!appended) return ::media::Result<std::size_t>::failure(appended.error());
    }
    return sizing.storageFootprint();
}

::media::Result<MediaWireDatagramBatchPartitionBuilder::Prepared>
MediaWireDatagramBatchPartitionBuilder::prepare(
    std::shared_ptr<const MediaWireGlobalSequenceState> identity,
    std::span<const MediaWireDatagramBatchInput> inputs, const MediaDatagramBatchPlan& plan,
    const MediaSharedAllocationGeometry& geometry,
    const std::shared_ptr<MediaNetworkMemoryResource>& resource, MediaNetworkStorageLease lease)
{
    using Result = ::media::Result<Prepared>;
    if (!identity || !identity->generation() || !resource)
        return Result::failure(::media::ErrorInfo::invalidArgument("Wire output preparation requires owners"));
    auto footprint = storageFootprint(inputs, plan, geometry);
    if (!footprint) return Result::failure(footprint.error());
    if (lease.bytes() != footprint.value()) return Result::failure(
        ::media::ErrorInfo::invalidArgument("Wire output reservation differs from its layout"));
    std::size_t count = 0;
    auto counted = visitPartitions(inputs, plan, [&](auto, const auto&) {
        ++count; return ::media::Status::success();
    });
    if (!counted) return Result::failure(counted.error());
    auto partitions = takeArray<Partition>(resource, lease, count);
    if (!partitions) return Result::failure(partitions.error());
    auto output = takeArray<std::shared_ptr<MediaWireDatagramBatchBuffer>>(resource, lease, count);
    if (!output) return Result::failure(output.error());
    auto built = visitPartitions(inputs, plan, [&](auto partition, const auto& layout) -> ::media::Status {
        auto payloadBlock = takeBacking(resource, lease, static_cast<std::size_t>(layout.bytes), alignof(std::uint8_t));
        if (!payloadBlock) return ::media::Status::failure(payloadBlock.error());
        auto payload = MediaOwnedByteStorage::fromAllocation(std::move(payloadBlock).value());
        if (!payload) return ::media::Status::failure(payload.error());
        auto descriptors = takeArray<MediaWireDatagram>(resource, lease, partition.size());
        if (!descriptors) return ::media::Status::failure(descriptors.error());
        auto control = takeBacking(resource, lease, geometry.bytes, geometry.alignment);
        if (!control) return ::media::Status::failure(control.error());
        auto batch = MediaWireDatagramBatchBuffer::prepare(identity, partition, std::move(payload).value(),
            std::move(descriptors).value(), geometry, resource, std::move(control).value());
        if (!batch) return ::media::Status::failure(batch.error());
        partitions.value().push_back(Partition{std::move(batch).value(), partition.size()});
        output.value().push_back(nullptr);
        return ::media::Status::success();
    });
    if (!built) return Result::failure(built.error());
    if (lease.bytes()) return Result::failure(::media::ErrorInfo::internalError("Wire output reservation was not consumed"));
    return Result::success(Prepared(identity->generation(), inputs.size(),
        std::move(partitions).value(), std::move(output).value()));
}

::media::Result<MediaWireDatagramBatchCollection>
MediaWireDatagramBatchPartitionBuilder::Prepared::bind(
    MediaDatagramCommitTransaction transaction) &&
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    auto partitions = std::move(m_partitions);
    auto output = std::move(m_output);
    const auto count = std::exchange(m_datagrams, 0);
    if (!transaction.valid() || transaction.generation() != m_generation ||
        !count || transaction.size() != count || partitions.empty() ||
        output.size() != partitions.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire partition preparation differs from its commit transaction"));
    }
    for (std::size_t index = 0; index < partitions.size(); ++index) {
        auto slice = transaction.takeNextSlice(partitions[index].datagrams);
        if (!slice) return Result::failure(slice.error());
        auto bound = std::move(partitions[index].buffer).bind(std::move(slice).value());
        if (!bound) return Result::failure(bound.error());
        output[index] = std::move(bound).value();
    }
    return Result::success(std::move(output));
}

} // namespace media::ffmpeg::graph
