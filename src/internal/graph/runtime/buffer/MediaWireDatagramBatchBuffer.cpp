#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaDatagramSubmitCommitLease::commit()
{
    if (!m_reservation) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "datagram submit commit lease cannot be committed twice or after move"));
    }
    auto reservation = std::move(m_reservation);
    return reservation->commit();
}

MediaWireDatagram::MediaWireDatagram(
    std::span<const std::uint8_t> bytes,
    const MediaWireDatagramDescriptor& descriptor,
    MediaDatagramSubmitCommitLease commitLease) noexcept
    : m_bytes(bytes),
      m_generation(descriptor.generation),
      m_endpointId(descriptor.endpointId),
      m_canonicalRelease(descriptor.canonicalRelease),
      m_canonicalDeadline(descriptor.canonicalDeadline),
      m_globalSequence(descriptor.globalSequence),
      m_commitLease(std::move(commitLease))
{
}

MediaDatagramSubmitCommitLease
MediaWireDatagram::takeCommitLease() noexcept
{
    return std::move(m_commitLease);
}

MediaWireDatagramBatchBuffer::MediaWireDatagramBatchBuffer(
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagram> datagrams) noexcept
    : m_generation(generation),
      m_payload(std::move(payload)),
      m_datagrams(std::move(datagrams))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::WireDatagramBatch);
    setDiagnosticName("wire_datagram_batch");
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaWireDatagramBatchBuffer::create(
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagramBatchEntry> entries)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    if (payload.empty() || entries.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch requires payload and entries"));
    }
    std::vector<MediaWireDatagram> datagrams;
    try {
        datagrams.reserve(entries.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire datagram entries"));
    }

    const auto generation = entries.front().descriptor.generation;
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    std::uint64_t expectedOffset = 0;
    std::optional<std::uint64_t> previousSequence;
    std::optional<MediaRunningTime> previousRelease;
    std::optional<MediaRunningTime> previousDeadline;
    for (auto& entry : entries) {
        const auto& descriptor = entry.descriptor;
        if (generation == 0 || descriptor.generation != generation ||
            descriptor.endpointId == 0 || descriptor.payloadSize == 0 ||
            descriptor.payloadOffset != expectedOffset ||
            descriptor.payloadOffset > payload.size() ||
            descriptor.payloadSize >
                payload.size() - descriptor.payloadOffset ||
            descriptor.canonicalRelease < zero ||
            descriptor.canonicalDeadline < descriptor.canonicalRelease ||
            (previousSequence &&
             descriptor.globalSequence <= *previousSequence) ||
            (previousRelease &&
             descriptor.canonicalRelease < *previousRelease) ||
            (previousDeadline &&
             descriptor.canonicalDeadline < *previousDeadline) ||
            !entry.commitLease.matches(
                descriptor.generation, descriptor.globalSequence)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "wire datagram batch violates payload, generation, sequence, deadline, or lease ownership"));
        }
        expectedOffset = descriptor.payloadOffset + descriptor.payloadSize;
        previousSequence = descriptor.globalSequence;
        previousRelease = descriptor.canonicalRelease;
        previousDeadline = descriptor.canonicalDeadline;
        try {
            datagrams.push_back(MediaWireDatagram(
                std::span<const std::uint8_t>(
                    payload.data() + descriptor.payloadOffset,
                    static_cast<std::size_t>(descriptor.payloadSize)),
                descriptor, std::move(entry.commitLease)));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "wire datagram batch entries"));
        }
    }
    if (expectedOffset != payload.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram entries must cover their payload exactly"));
    }
    try {
        return Result::success(std::shared_ptr<MediaWireDatagramBatchBuffer>(
            new MediaWireDatagramBatchBuffer(
                generation, std::move(payload), std::move(datagrams))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaWireDatagramBatchBuffer"));
    }
}

std::optional<std::uint64_t>
MediaWireDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return m_payload.size();
}

} // namespace media::ffmpeg::graph
