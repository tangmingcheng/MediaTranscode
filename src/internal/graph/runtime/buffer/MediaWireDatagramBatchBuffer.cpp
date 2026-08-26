#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptorValidator.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaDatagramSubmitCommitLease::markScheduled(
    MediaRunningTime now) noexcept
{
    return m_reservation
        ? m_reservation->markScheduled(now)
        : ::media::Status::failure(::media::ErrorInfo::internalError(
              "datagram submit commit lease is inactive"));
}

::media::Status MediaDatagramSubmitCommitLease::markSubmitted(
    MediaRunningTime now) noexcept
{
    return m_reservation
        ? m_reservation->markSubmitted(now)
        : ::media::Status::failure(::media::ErrorInfo::internalError(
              "datagram submit commit lease is inactive"));
}

::media::Status MediaDatagramSubmitCommitLease::commit(
    MediaRunningTime now) noexcept
{
    if (!m_reservation) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "datagram submit commit lease cannot be committed twice or after move"));
    }
    auto reservation = std::move(m_reservation);
    return reservation->commit(now);
}

MediaWireDatagram::MediaWireDatagram(
    std::span<const std::uint8_t> bytes,
    const MediaWireDatagramDescriptor& descriptor,
    MediaDatagramSubmitCommitLease commitLease) noexcept
    : m_bytes(bytes),
      m_descriptor(descriptor),
      m_commitLease(std::move(commitLease))
{
}

MediaWireDatagramBatchBuffer::MediaWireDatagramBatchBuffer(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagram> datagrams) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_payload(std::move(payload)),
      m_datagrams(std::move(datagrams))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::WireDatagramBatch);
    setDiagnosticName("wire_datagram_batch");
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaWireDatagramBatchBuffer::create(
    std::string sessionKey,
    std::string serviceScopeId,
    std::vector<std::uint8_t> payload,
    std::vector<MediaWireDatagramBatchEntry> entries)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    if (sessionKey.empty() || serviceScopeId.empty() || payload.empty() ||
        entries.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram batch requires service identity, payload, and entries"));
    }
    std::vector<MediaWireDatagram> datagrams;
    try {
        datagrams.reserve(entries.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire datagram entries"));
    }

    MediaWireDatagramDescriptorValidator validator(
        static_cast<std::uint64_t>(payload.size()));
    for (auto& entry : entries) {
        const auto& descriptor = entry.descriptor;
        auto validDescriptor = validator.accept(descriptor);
        if (!validDescriptor ||
            !entry.commitLease.matches(
                descriptor.generation, descriptor.globalSequence)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "wire datagram batch violates its shared descriptor or lease ownership"));
        }
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
    auto complete = validator.finish();
    if (!complete) return Result::failure(complete.error());
    try {
        return Result::success(std::shared_ptr<MediaWireDatagramBatchBuffer>(
            new MediaWireDatagramBatchBuffer(
                std::move(sessionKey), std::move(serviceScopeId),
                validator.generation(), std::move(payload),
                std::move(datagrams))));
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
