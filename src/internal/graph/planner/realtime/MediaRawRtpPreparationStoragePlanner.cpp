#include "internal/graph/planner/realtime/MediaRawRtpPreparationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRtcpPreparationStoragePlanner.h"
#include "internal/graph/runtime/buffer/MediaPreparedRtpDatagramStorage.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapterFactory.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"
#include "internal/graph/runtime/resource/MediaPreparedAllocationCost.h"
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRawRtpRuntimeStoragePlan>
MediaRawRtpPreparationStoragePlanner::runtimeStorage(
    std::span<const std::array<MediaRtpReceiveGeometry, 2>* const> streams)
{
    using Result = ::media::Result<MediaRawRtpRuntimeStoragePlan>;
    using Cost = MediaPreparedAllocationCost;
    if (streams.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP runtime storage requires original transport geometries"));
    std::size_t total = 0;
    for (const auto* channels : streams) {
        if (!channels || (*channels)[0].channel() != MediaRtpUdpChannel::Rtp ||
            (*channels)[1].channel() != MediaRtpUdpChannel::Rtcp ||
            (*channels)[0].maximumDatagramBytes() != (*channels)[1].maximumDatagramBytes())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP runtime storage requires paired channel geometries"));
        // Mixed receive may still be outside the Buffer lock at the handoff.
        // Cover both its lazy scratch and its one returned owned datagram,
        // without assuming either allocation was already present in held bytes.
        auto ordinary = MediaPreparationMemoryResource::allocationFootprint(
            (*channels)[0].maximumDatagramBytes(), alignof(std::uint8_t));
        auto next = Cost::sum({::media::Result<std::size_t>::success(total), ordinary, ordinary,
            Cost::shared<MediaRtpUdpTransport>(),
            MediaRtpIngressAdapterFactory::preparationStorageBytes((*channels)[0]),
            MediaRtpIngressAdapterFactory::preparationStorageBytes((*channels)[1])});
        if (!next) return Result::failure(next.error());
        total = next.value();
    }
    return Result::success({total, streams.size()});
}

::media::Result<MediaRawRtpPreparationStoragePlan>
MediaRawRtpPreparationStoragePlanner::plan(std::size_t wireBytes,
    std::span<const std::size_t> maximumDatagramBytes)
{
    using Result = ::media::Result<MediaRawRtpPreparationStoragePlan>;
    if (!wireBytes || maximumDatagramBytes.empty())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP preparation requires a wire limit and transport datagram bounds"));
    // Transport rejects empty datagrams. Every retained descriptor therefore
    // consumes at least one of the cumulative wire bytes; no packet-size sample
    // or codec-specific minimum is used. Each stream can own the whole prefix.
    auto rtcp = MediaRtcpPreparationStoragePlanner::plan(maximumDatagramBytes, 0, MediaRtcpPreparationUse::ParseOnly);
    if (!rtcp) return Result::failure(rtcp.error());
    auto bootstrap = MediaPreparationMemoryResource::bootstrapStorageBytes();
    if (!bootstrap) return Result::failure(bootstrap.error());
    auto entryFacts = MediaPreparedRtpDatagramStorage::allocationFacts();
    if (!entryFacts) return Result::failure(entryFacts.error());
    auto entryBlock = MediaPreparationMemoryResource::allocationFootprint(
        entryFacts.value().bytes, entryFacts.value().alignment);
    if (!entryBlock) return Result::failure(entryBlock.error());
    auto entries = MediaCheckedArithmetic::multiply(wireBytes,
        entryBlock.value(), "RTP preparation stable entries");
    if (!entries) return Result::failure(entries.error());
    auto descriptors = MediaCheckedArithmetic::multiply(wireBytes,
        sizeof(MediaPreparedRtpDatagramOwner), "RTP preparation owner references");
    if (!descriptors || descriptors.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP preparation descriptors exceed addressable memory"));
    auto descriptorBlock = MediaPreparationMemoryResource::allocationFootprint(
        static_cast<std::size_t>(descriptors.value()), alignof(MediaPreparedRtpDatagramOwner));
    auto payloadHeader = MediaPreparationMemoryResource::allocationFootprint(0, alignof(std::uint8_t));
    if (!descriptorBlock || !payloadHeader)
        return Result::failure(!descriptorBlock ? descriptorBlock.error() : payloadHeader.error());
    auto headers = MediaCheckedArithmetic::multiply(wireBytes,
        payloadHeader.value(), "RTP preparation payload headers");
    if (!headers) return Result::failure(headers.error());
    auto total = MediaCheckedArithmetic::add(wireBytes, headers.value(),
        "RTP preparation retained payload and headers");
    if (!total) return Result::failure(total.error());
    // Entries share the cumulative AV wire bound. The budget admits only one
    // probe lease at a time, so at most one full owner-reference snapshot exists.
    for (const auto allocation : {entries.value(), static_cast<std::uint64_t>(descriptorBlock.value()),
            static_cast<std::uint64_t>(bootstrap.value()),
            static_cast<std::uint64_t>(rtcp.value().parserTransientBytes)}) {
        total = MediaCheckedArithmetic::add(total.value(), allocation,
            "RTP preparation stable entries and probe snapshot");
        if (!total) return Result::failure(total.error());
    }
    for (const auto maximum : maximumDatagramBytes) {
        if (!maximum) return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP preparation transport has no datagram bound"));
        auto ingress = MediaRtpIngressAdapterFactory::maximumPreparationStorageBytes(wireBytes, maximum);
        auto receiverOwner = MediaPreparedAllocationCost::shared<MediaRawRtpPreparedInputBuffer>();
        auto workers = MediaRawRtpPreparedInputBuffer::workerStorageBytes();
        if (!ingress || !receiverOwner || !workers)
            return Result::failure(!ingress ? ingress.error() : !receiverOwner ? receiverOwner.error() : workers.error());
        for (const auto allocation : {ingress.value(), receiverOwner.value(), workers.value()}) {
            total = MediaCheckedArithmetic::add(total.value(), allocation,
                "RTP preparation dual-channel ingress and shared owner");
            if (!total) return Result::failure(total.error());
        }
        auto datagram = MediaPreparationMemoryResource::allocationFootprint(maximum, alignof(std::uint8_t));
        if (!datagram) return Result::failure(datagram.error());
        // One receiver scratch and one not-yet-observed payload can coexist per
        // stream. Queued payload shares the single wire limit across AV.
        for (const auto allocation : {descriptorBlock.value(), datagram.value(), datagram.value()}) {
            total = MediaCheckedArithmetic::add(total.value(), allocation,
                "RTP preparation simultaneous allocation storage");
            if (!total) return Result::failure(total.error());
        }
    }
    if (total.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP preparation storage exceeds addressable memory"));
    return Result::success({wireBytes, wireBytes, maximumDatagramBytes.size(),
        static_cast<std::size_t>(total.value()), entryFacts.value()});
}

} // namespace media::ffmpeg::graph
