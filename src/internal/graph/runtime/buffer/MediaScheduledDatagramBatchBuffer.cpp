#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuffer.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaScheduledDatagram::MediaScheduledDatagram(
    std::span<const std::uint8_t> bytes,
    MediaRunningTime enqueueNotBefore,
    MediaRunningTime enqueueDeadline,
    MediaRunningTime serviceDuration) noexcept
    : m_bytes(bytes),
      m_enqueueNotBefore(enqueueNotBefore),
      m_enqueueDeadline(enqueueDeadline),
      m_serviceDuration(serviceDuration)
{
}

MediaScheduledDatagramBatchBuffer::MediaScheduledDatagramBatchBuffer(
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaScheduledDatagram> datagrams) noexcept
    : m_generation(generation),
      m_payload(std::move(payload)),
      m_datagrams(std::move(datagrams))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::ScheduledDatagramBatch);
    setDiagnosticName("scheduled_datagram_batch");
}

::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>>
MediaScheduledDatagramBatchBuffer::create(
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaScheduledDatagramDescriptor> descriptors)
{
    using Result = ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>>;
    if (generation == 0 || payload.empty() || descriptors.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch requires a generation and datagrams"));
    }
    std::vector<MediaScheduledDatagram> datagrams;
    try {
        datagrams.reserve(descriptors.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "scheduled datagram descriptors"));
    }
    std::uint64_t expectedOffset = 0;
    std::optional<MediaRunningTime> previousCompletion;
    std::optional<MediaRunningTime> previousDeadline;
    for (const auto& descriptor : descriptors) {
        auto completion = descriptor.enqueueNotBefore.checkedAdd(
            descriptor.serviceDuration);
        if (descriptor.payloadSize == 0 ||
            descriptor.payloadOffset != expectedOffset ||
            descriptor.payloadOffset > payload.size() ||
            descriptor.payloadSize > payload.size() - descriptor.payloadOffset ||
            descriptor.enqueueNotBefore < MediaRunningTime::fromNanoseconds(0) ||
            descriptor.enqueueDeadline < descriptor.enqueueNotBefore ||
            descriptor.serviceDuration <= MediaRunningTime::fromNanoseconds(0) ||
            (previousCompletion &&
             descriptor.enqueueNotBefore < *previousCompletion) ||
            (previousDeadline &&
             descriptor.enqueueDeadline < *previousDeadline) ||
            !completion) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram batch payload or timing is not contiguous and ordered"));
        }
        expectedOffset = descriptor.payloadOffset + descriptor.payloadSize;
        previousCompletion = completion.value();
        previousDeadline = descriptor.enqueueDeadline;
        datagrams.push_back(MediaScheduledDatagram(
            std::span<const std::uint8_t>(
                payload.data() + descriptor.payloadOffset,
                static_cast<std::size_t>(descriptor.payloadSize)),
            descriptor.enqueueNotBefore,
            descriptor.enqueueDeadline,
            descriptor.serviceDuration));
    }
    if (expectedOffset != payload.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram descriptors must cover their payload exactly"));
    }
    try {
        return Result::success(std::shared_ptr<MediaScheduledDatagramBatchBuffer>(
            new MediaScheduledDatagramBatchBuffer(
                generation, std::move(payload), std::move(datagrams))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledDatagramBatchBuffer"));
    }
}

std::optional<std::uint64_t>
MediaScheduledDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
