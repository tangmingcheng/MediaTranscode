#include "internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.h"

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
      m_generation(descriptor.generation),
      m_endpointId(descriptor.endpointId),
      m_canonicalRelease(descriptor.canonicalRelease),
      m_canonicalDeadline(descriptor.canonicalDeadline),
      m_globalSequence(descriptor.globalSequence),
      m_enqueueNotBefore(descriptor.enqueueNotBefore),
      m_enqueueNotAfter(descriptor.enqueueNotAfter),
      m_wireServiceDuration(descriptor.wireServiceDuration),
      m_commitLease(std::move(commitLease))
{
}

::media::Status MediaScheduledWireDatagram::commitSubmit()
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

    const auto generation = entries.front().descriptor.generation;
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    std::uint64_t expectedOffset = 0;
    std::optional<std::uint64_t> previousSequence;
    std::optional<MediaRunningTime> previousRelease;
    std::optional<MediaRunningTime> previousDeadline;
    std::optional<MediaRunningTime> previousCompletion;
    std::optional<MediaRunningTime> previousEnqueueNotAfter;
    for (auto& entry : entries) {
        const auto& descriptor = entry.descriptor;
        const auto* endpoint = plan.endpoint(descriptor.endpointId);
        auto plannedDatagram = plan.validateDatagram(
            descriptor.endpointId, descriptor.payloadSize);
        auto completion = descriptor.enqueueNotBefore.checkedAdd(
            descriptor.wireServiceDuration);
        auto residence = descriptor.enqueueNotAfter.checkedSubtract(
            descriptor.canonicalRelease);
        if (generation == 0 || generation != plan.generation() ||
            descriptor.generation != generation || !endpoint ||
            !plannedDatagram || descriptor.payloadSize == 0 ||
            descriptor.payloadOffset != expectedOffset ||
            descriptor.payloadOffset > payload.size() ||
            descriptor.payloadSize >
                payload.size() - descriptor.payloadOffset ||
            descriptor.canonicalRelease < zero ||
            descriptor.canonicalDeadline < descriptor.canonicalRelease ||
            descriptor.enqueueNotBefore < descriptor.canonicalRelease ||
            descriptor.enqueueNotAfter < descriptor.enqueueNotBefore ||
            descriptor.enqueueNotAfter > descriptor.canonicalDeadline ||
            descriptor.wireServiceDuration <= zero || !completion ||
            !residence ||
            residence.value() > endpoint->maximumResidence ||
            residence.value() > plan.backlog().maximumResidence ||
            (previousSequence &&
             descriptor.globalSequence <= *previousSequence) ||
            (previousRelease &&
             descriptor.canonicalRelease < *previousRelease) ||
            (previousDeadline &&
             descriptor.canonicalDeadline < *previousDeadline) ||
            (previousCompletion &&
             descriptor.enqueueNotBefore < *previousCompletion) ||
            (previousEnqueueNotAfter &&
             descriptor.enqueueNotAfter < *previousEnqueueNotAfter) ||
            !entry.commitLease.matches(
                descriptor.generation, descriptor.globalSequence)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire datagram violates plan, payload, generation, sequence, deadline, or lease ownership"));
        }

        EndpointBatchUsage* usage = nullptr;
        try {
            usage = &endpointUsage[descriptor.endpointId];
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "scheduled wire endpoint batch accounting"));
        }
        if (usage->datagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            descriptor.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() - usage->bytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire endpoint batch accounting overflowed"));
        }
        ++usage->datagrams;
        usage->bytes += descriptor.payloadSize;
        if (usage->datagrams > endpoint->maximumPendingDatagrams ||
            usage->bytes > endpoint->maximumPendingBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled wire endpoint batch exceeds its hard bound"));
        }

        expectedOffset = descriptor.payloadOffset + descriptor.payloadSize;
        previousSequence = descriptor.globalSequence;
        previousRelease = descriptor.canonicalRelease;
        previousDeadline = descriptor.canonicalDeadline;
        previousCompletion = completion.value();
        previousEnqueueNotAfter = descriptor.enqueueNotAfter;
        try {
            datagrams.push_back(MediaScheduledWireDatagram(
                std::span<const std::uint8_t>(
                    payload.data() + descriptor.payloadOffset,
                    static_cast<std::size_t>(descriptor.payloadSize)),
                descriptor, std::move(entry.commitLease)));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "scheduled wire datagram batch entries"));
        }
    }
    if (expectedOffset != payload.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled wire datagram entries must cover their payload exactly"));
    }
    try {
        return Result::success(
            std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>(
                new MediaScheduledWireDatagramBatchBuffer(
                    generation, std::move(payload),
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
