#include "internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

#include <limits>
#include <new>
#include <sstream>
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
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::ScheduledWireDatagramBatch);
    setDiagnosticName("scheduled_wire_datagram_batch");
}

::media::Result<std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
MediaScheduledWireDatagramBatchBuffer::create(
    const MediaDatagramShapingPlan& plan,
    MediaWireDatagramBatchBuffer& source,
    std::vector<MediaScheduledWireDatagramDescriptor> descriptors)
{
    using Result = ::media::Result<
        std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>;
    if (source.m_sessionKey != plan.sessionKey() ||
        source.m_serviceScopeId != plan.serviceScope().scopeId ||
        source.m_generation != plan.generation() || source.m_payload.empty() ||
        source.m_datagrams.empty() ||
        descriptors.size() != source.m_datagrams.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled wire datagram batch violates its service identity or descriptor cardinality"));
    }
    if (descriptors.size() > plan.batch().maximumDatagrams ||
        source.m_payload.size() > plan.batch().maximumBytes) {
        std::ostringstream message;
        message << "scheduled wire datagram batch exceeds planner partition"
                << " datagrams=" << descriptors.size()
                << " maximum_datagrams=" << plan.batch().maximumDatagrams
                << " bytes=" << source.m_payload.size()
                << " maximum_bytes=" << plan.batch().maximumBytes;
        return Result::failure(::media::ErrorInfo::invalidArgument(
            message.str()));
    }

    std::vector<MediaScheduledWireDatagram> datagrams;
    std::unordered_map<std::uint64_t, EndpointBatchUsage> endpointUsage;
    try {
        datagrams.reserve(descriptors.size());
        endpointUsage.reserve(plan.endpoints().size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "scheduled wire datagram validation state"));
    }

    MediaWireDatagramDescriptorValidator validator(
        static_cast<std::uint64_t>(source.m_payload.size()));
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    std::optional<MediaRunningTime> previousCompletion;
    std::optional<MediaRunningTime> previousEnqueueNotAfter;
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        const auto& wire = descriptor.wire;
        const auto& datagram = source.m_datagrams[index];
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
            datagram.m_descriptor != wire ||
            !datagram.m_commitLease.matches(
                wire.generation, wire.globalSequence)) {
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
    }
    auto complete = validator.finish();
    if (!complete) return Result::failure(complete.error());

    std::shared_ptr<MediaScheduledWireDatagramBatchBuffer> output;
    try {
        output = std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>(
            new MediaScheduledWireDatagramBatchBuffer(
                source.m_sessionKey, source.m_serviceScopeId,
                validator.generation()));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledWireDatagramBatchBuffer"));
    }
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& wire = descriptors[index].wire;
        datagrams.push_back(MediaScheduledWireDatagram(
            std::span<const std::uint8_t>(
                source.m_payload.data() + wire.payloadOffset,
                static_cast<std::size_t>(wire.payloadSize)),
            descriptors[index],
            std::move(source.m_datagrams[index].m_commitLease)));
    }
    output->m_payload = std::move(source.m_payload);
    output->m_datagrams = std::move(datagrams);
    return Result::success(std::move(output));
}

std::optional<std::uint64_t>
MediaScheduledWireDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
