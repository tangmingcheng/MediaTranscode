#pragma once
#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramPartitionLayout.h"

namespace media::ffmpeg::graph {
using MediaWireDatagramBatchCollection =
    MediaFixedCapacityStorage<std::shared_ptr<MediaWireDatagramBatchBuffer>>;

class MediaWireDatagramBatchPartitionBuilder final {
    struct Partition final {
        MediaWireDatagramBatchBuffer::Prepared buffer;
        std::size_t datagrams;
    };
public:
    // Incremental sizing accepts protocol facts before writer arrays exist.
    // It uses the same partition cursor as construction and owns no payload.
    class Sizing final {
    public:
        Sizing(const MediaDatagramBatchPlan& plan,
            const MediaSharedAllocationGeometry& geometry) noexcept
            : m_plan(plan), m_geometry(geometry) {}
        ::media::Status append(std::size_t bytes, MediaRunningTime deadline);
        ::media::Result<std::size_t> storageFootprint() const;
    private:
        ::media::Status accountPartition(const MediaWireDatagramPartitionLayout& layout);
        MediaDatagramBatchPlan m_plan;
        MediaSharedAllocationGeometry m_geometry;
        MediaWireDatagramPartitionLayout m_layout;
        std::size_t m_total = 0;
        std::size_t m_partitions = 0;
    };
    static ::media::Result<std::size_t> storageOverheadContribution(
        std::size_t datagrams, const MediaSharedAllocationGeometry& geometry);
    // Published backing only: payload, descriptors, and the shared batch object.
    static ::media::Result<std::size_t> partitionStorageFootprint(
        std::size_t datagrams, std::size_t payloadBytes,
        const MediaSharedAllocationGeometry& geometry);
    static ::media::Result<std::size_t> publishedStorageUpperBound(
        std::size_t datagrams, std::size_t payloadBytes,
        const MediaSharedAllocationGeometry& geometry);
    static ::media::Result<std::size_t> publicationCollectionStorageFootprint(
        std::size_t partitions);
    class Prepared final {
    public:
        Prepared(Prepared&&) noexcept = default;
        Prepared& operator=(Prepared&&) noexcept = default;
        Prepared(const Prepared&) = delete;
        Prepared& operator=(const Prepared&) = delete;
        ::media::Result<MediaWireDatagramBatchCollection> bind(
            MediaDatagramCommitTransaction transaction) &&;
    private:
        friend class MediaWireDatagramBatchPartitionBuilder;
        Prepared(std::uint64_t generation, std::size_t datagrams,
            MediaFixedCapacityStorage<Partition> partitions,
            MediaWireDatagramBatchCollection output) noexcept
            : m_generation(generation), m_datagrams(datagrams),
              m_partitions(std::move(partitions)), m_output(std::move(output)) {}
        std::uint64_t m_generation;
        std::size_t m_datagrams;
        MediaFixedCapacityStorage<Partition> m_partitions;
        MediaWireDatagramBatchCollection m_output;
    };
    static ::media::Result<std::size_t> storageFootprint(
        std::span<const MediaWireDatagramBatchInput> inputs,
        const MediaDatagramBatchPlan& plan,
        const MediaSharedAllocationGeometry& geometry);
    static ::media::Result<Prepared> prepare(
        std::shared_ptr<const MediaWireGlobalSequenceState> identity,
        std::span<const MediaWireDatagramBatchInput> inputs,
        const MediaDatagramBatchPlan& plan,
        const MediaSharedAllocationGeometry& geometry,
        const std::shared_ptr<MediaNetworkMemoryResource>& resource,
        MediaNetworkStorageLease lease);
};
} // namespace media::ffmpeg::graph
