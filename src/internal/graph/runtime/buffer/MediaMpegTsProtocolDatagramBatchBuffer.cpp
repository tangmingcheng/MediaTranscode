#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include <algorithm>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class MediaTsProtocolCommitReservation final {
public:
    MediaTsProtocolCommitReservation(
        MediaTsPacketCursor cursor,
        MediaTsPacketCommitToken token,
        std::size_t entryCount) noexcept
        : m_cursor(std::move(cursor)),
          m_token(std::move(token)),
          m_entryCount(entryCount)
    {
    }

    MediaTsProtocolCommitReservation(
        MediaTsProtocolCommitReservation&& other) noexcept
        : m_cursor(std::move(other.m_cursor)),
          m_token(std::move(other.m_token)),
          m_entryCount(other.m_entryCount),
          m_nextEntry(other.m_nextEntry)
    {
        other.m_nextEntry = other.m_entryCount;
    }
    MediaTsProtocolCommitReservation& operator=(
        MediaTsProtocolCommitReservation&&) = delete;

    ~MediaTsProtocolCommitReservation() noexcept
    {
        if (m_nextEntry != m_entryCount) m_cursor.poison();
    }

    std::size_t size() const noexcept { return m_entryCount; }

    ::media::Status commitNextPrefix(std::size_t count) noexcept
    {
        if (count > m_entryCount - m_nextEntry) {
            m_cursor.poison();
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS protocol commit prefix exceeds its transaction"));
        }
        m_nextEntry += count;
        if (m_nextEntry != m_entryCount) return ::media::Status::success();
        auto committed = m_cursor.commit(std::move(m_token));
        if (!committed) m_cursor.poison();
        return committed;
    }

private:
    MediaTsPacketCursor m_cursor;
    MediaTsPacketCommitToken m_token;
    std::size_t m_entryCount;
    std::size_t m_nextEntry = 0;
};

} // namespace

MediaMpegTsProtocolDatagram::MediaMpegTsProtocolDatagram(
    std::span<const std::uint8_t> bytes,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline) noexcept
    : m_bytes(bytes),
      m_presentationOnMaster(presentationOnMaster),
      m_canonicalRelease(canonicalRelease),
      m_canonicalDeadline(canonicalDeadline)
{
}

MediaMpegTsProtocolDatagramBatchBuffer::
MediaMpegTsProtocolDatagramBatchBuffer(std::uint64_t generation) noexcept
    : m_generation(generation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::MpegTsProtocolDatagramBatch);
    setDiagnosticName("mpegts_protocol_datagram_batch");
}

::media::Result<std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
MediaMpegTsProtocolDatagramBatchBuffer::create(
    std::uint64_t generation,
    MediaTsPacketCursor cursor,
    std::uint16_t maximumPacketsPerDatagram,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    using Result = ::media::Result<
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>;
    if (generation == 0 || maximumPacketsPerDatagram < 1 ||
        presentationOnMaster < MediaRunningTime::fromNanoseconds(0) ||
        canonicalRelease < MediaRunningTime::fromNanoseconds(0) ||
        canonicalDeadline < canonicalRelease) {
        cursor.poison();
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS protocol batch requires generation, packet geometry, and canonical timing"));
    }
    auto prepared = cursor.prepareRemaining();
    if (!prepared) {
        cursor.poison();
        return Result::failure(prepared.error());
    }
    const auto packets = prepared.value().packets();
    const std::size_t entryCount =
        (packets.size() + maximumPacketsPerDatagram - 1) /
        maximumPacketsPerDatagram;
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> output;
    try {
        output = std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>(
            new MediaMpegTsProtocolDatagramBatchBuffer(generation));
        output->m_payload.reserve(packets.size() * std::size_t{188});
        output->m_datagrams.reserve(entryCount);
        for (const auto& packet : packets) {
            output->m_payload.insert(
                output->m_payload.end(), packet.begin(), packet.end());
        }
        std::size_t packetOffset = 0;
        for (std::size_t index = 0; index < entryCount; ++index) {
            const std::size_t packetCount = (std::min)(
                static_cast<std::size_t>(maximumPacketsPerDatagram),
                packets.size() - packetOffset);
            const std::size_t byteOffset = packetOffset * std::size_t{188};
            const std::size_t byteCount = packetCount * std::size_t{188};
            output->m_datagrams.push_back(MediaMpegTsProtocolDatagram(
                std::span<const std::uint8_t>(
                    output->m_payload.data() + byteOffset, byteCount),
                presentationOnMaster, canonicalRelease, canonicalDeadline));
            packetOffset += packetCount;
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS protocol datagram batch"));
    }
    auto transaction = MediaProtocolDatagramCommitTransaction::create(
        MediaTsProtocolCommitReservation(
            std::move(cursor), prepared.value().takeCommitToken(), entryCount));
    if (!transaction) return Result::failure(transaction.error());
    output->m_commitTransaction.emplace(std::move(transaction).value());
    return Result::success(std::move(output));
}

::media::Result<MediaProtocolDatagramCommitTransaction>
MediaMpegTsProtocolDatagramBatchBuffer::takeCommitTransaction() noexcept
{
    using Result = ::media::Result<MediaProtocolDatagramCommitTransaction>;
    if (!m_commitTransaction || !m_commitTransaction->valid()) {
        return Result::failure(
            ::media::ErrorInfo::internalError(
                "MPEG-TS protocol transaction is absent or already moved"));
    }
    auto transaction = std::move(*m_commitTransaction);
    m_commitTransaction.reset();
    return Result::success(std::move(transaction));
}

std::optional<std::uint64_t>
MediaMpegTsProtocolDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return static_cast<std::uint64_t>(m_payload.size());
}

} // namespace media::ffmpeg::graph
