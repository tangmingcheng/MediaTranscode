#include "internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

#include <limits>
#include <new>
#include <unordered_map>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct EndpointBatchUsage final {
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
};

} // namespace

MediaScheduledWireDatagram::MediaScheduledWireDatagram(
    std::span<const std::uint8_t> bytes,
    const MediaScheduledWireDatagramDescriptor& descriptor,
    MediaDatagramSubmitCommitLease commitLease) noexcept
    : m_bytes(bytes),
      m_descriptor(descriptor),
      m_commitLease(std::move(commitLease))
{
}

::media::Status MediaScheduledWireDatagram::commitSubmit() noexcept
{
    return m_commitLease.commit();
}

MediaScheduledWireDatagramBatchBuffer::
MediaScheduledWireDatagramBatchBuffer(
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaScheduledWireDatagram> datagrams) noexcept
    : m_generation(generation),
      m_payload(std::move(payload)),
      m_datagrams(std::move(datagrams))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::ScheduledWireDatagramBatch);
    setDiagnosticName("scheduled_wire_datagram_batch");
}

::media::Result<std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
MediaScheduledWireDatagramBatchBuffer::create(
    const MediaDatagramShapingPlan& plan,
    std::vector<std::uint8_t> payload,
    std::vector<MediaScheduledWireDatagramBatchEntry> entries)
{
    using Result = ::media::Result<
        std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>;
    if (payload.empty() || entries.empty() ||
        entries.size() > plan.batch().maximumDatagrams ||
        payload.size() > plan.batch().maximumBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled wire datagram batch exceeds its planner-owned hard bounds"));
    }

    std::vector<MediaScheduledWireDatagram> datagrams;
    std::unordered_map<std::uint64_t, EndpointBatchUsage> endpointUsage;
    try {
        datagrams.reserve(entries.size());
        endpointUsage.reserve(plan.endpoints().size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "scheduled wire datagram validation state"));
    }

    MediaWireDatagramDescriptorValidator validator(
        static_cast<std::uint64_t>(payload.size()));
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    std::optional<MediaRunningTime> previousCompletion;
    std::optional<MediaRunningTime> previousEnqueueNotAfter;
    for (auto& entry : entries) {
        const auto& descriptor = entry.descriptor;
        const auto& wire = descriptor.wire;
        const auto* endpoint = plan.endpoint(wire.endpointId);
        auto validWire = validator.accept(wire);
        auto plannedWireCost = plan.plannedWireCost(
            wire.endpointId, wire.payloadSize);
        auto completion = descriptor.enqueueNotBefore.checkedAdd(
            descriptor.wireServiceDuration);
        auto residence = descriptor.enqueueNotAfter.checkedSubtract(
            wire.canonicalRelease);
        if (!validWire || wire.generation != plan.generation() || !endpoint ||
            !plannedWireCost || descriptor.wireServiceDuration !=
                                    plannedWireCost.value().peakServiceDuration ||
            descriptor.enqueueNotBefore < wire.canonicalRelease ||
            descriptor.enqueueNotAfter < descriptor.enqueueNotBefore ||
            descriptor.enqueueNotAfter > wire.canonicalDeadline ||
            descriptor.wireServiceDuration <= zero || !completion ||
            !residence ||
            residence.value() > endpoint->maximumResidence ||
            residence.value() > plan.backlog().maximumResidence ||
            (previousCompletion &&
             descriptor.enqueueNotBefore < *previousCompletion) ||
            (previousEnqueueNotAfter &&
             descriptor.enqueueNotAfter < *previousEnqueueNotAfter) ||
            !entry.commitLease.matches(wire.generation, wire.globalSequence)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire datagram violates plan, payload, generation, sequence, deadline, or lease ownership"));
        }

        EndpointBatchUsage* usage = nullptr;
        try {
            usage = &endpointUsage[wire.endpointId];
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "scheduled wire endpoint batch accounting"));
        }
        if (usage->datagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            wire.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() - usage->bytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire endpoint batch accounting overflowed"));
        }
        ++usage->datagrams;
        usage->bytes += wire.payloadSize;
        if (usage->datagrams > endpoint->maximumPendingDatagrams ||
            usage->bytes > endpoint->maximumPendingBytes ||
            usage->bytes > endpoint->socketHardBoundBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire endpoint batch exceeds its pending or socket hard bound"));
        }

        previousCompletion = completion.value();
        previousEnqueueNotAfter = descriptor.enqueueNotAfter;
        try {
            datagrams.push_back(MediaScheduledWireDatagram(
                std::span<const std::uint8_t>(
                    payload.data() + wire.payloadOffset,
                    static_cast<std::size_t>(wire.payloadSize)),
                descriptor, std::move(entry.commitLease)));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "scheduled wire datagram batch entries"));
        }
    }
    auto complete = validator.finish();
    if (!complete) return Result::failure(complete.error());
    try {
        return Result::success(
            std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>(
                new MediaScheduledWireDatagramBatchBuffer(
                    validator.generation(), std::move(payload),
                    std::move(datagrams))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledWireDatagramBatchBuffer"));
    }
}

std::optional<std::uint64_t>
MediaScheduledWireDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
