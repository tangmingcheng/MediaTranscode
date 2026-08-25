#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include <algorithm>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class MediaTsProtocolCommitTransaction final {
public:
    MediaTsProtocolCommitTransaction(
        MediaTsPacketCursor cursor,
        MediaTsPacketCommitToken token,
        std::size_t entryCount) noexcept
        : m_cursor(std::move(cursor)),
          m_token(std::move(token)),
          m_entryCount(entryCount)
    {
    }

    ~MediaTsProtocolCommitTransaction() noexcept
    {
        if (m_nextEntry != m_entryCount) m_cursor.poison();
    }

    ::media::Status commit(std::size_t index) noexcept
    {
        if (index != m_nextEntry || index >= m_entryCount) {
            m_cursor.poison();
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS protocol datagram commit is stale or reordered"));
        }
        ++m_nextEntry;
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

class MediaTsProtocolEntryReservation final {
public:
    MediaTsProtocolEntryReservation(
        std::shared_ptr<MediaTsProtocolCommitTransaction> transaction,
        std::size_t index) noexcept
        : m_transaction(std::move(transaction)), m_index(index)
    {
    }

    ::media::Status commit() noexcept
    {
        return m_transaction
            ? m_transaction->commit(m_index)
            : ::media::Status::failure(::media::ErrorInfo::internalError(
                  "MPEG-TS protocol entry lost its transaction"));
    }

private:
    std::shared_ptr<MediaTsProtocolCommitTransaction> m_transaction;
    std::size_t m_index;
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
    std::shared_ptr<MediaTsProtocolCommitTransaction> transaction;
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> output;
    try {
        transaction = std::make_shared<MediaTsProtocolCommitTransaction>(
            std::move(cursor), prepared.value().takeCommitToken(), entryCount);
        output = std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>(
            new MediaMpegTsProtocolDatagramBatchBuffer(generation));
        output->m_payload.reserve(packets.size() * std::size_t{188});
        output->m_datagrams.reserve(entryCount);
        output->m_commitLeases.reserve(entryCount);
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
            auto lease = MediaProtocolDatagramCommitLease::create(
                MediaTsProtocolEntryReservation(transaction, index));
            if (!lease) return Result::failure(lease.error());
            output->m_commitLeases.push_back(std::move(lease).value());
            packetOffset += packetCount;
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS protocol datagram batch"));
    }
    return Result::success(std::move(output));
}

::media::Result<MediaProtocolDatagramCommitLease>
MediaMpegTsProtocolDatagramBatchBuffer::takeCommitLease(
    std::size_t index) noexcept
{
    if (index >= m_commitLeases.size() || !m_commitLeases[index].valid()) {
        return ::media::Result<MediaProtocolDatagramCommitLease>::failure(
            ::media::ErrorInfo::internalError(
                "MPEG-TS protocol datagram lease is absent or already moved"));
    }
    return ::media::Result<MediaProtocolDatagramCommitLease>::success(
        std::move(m_commitLeases[index]));
}

std::optional<std::uint64_t>
MediaMpegTsProtocolDatagramBatchBuffer::payloadFootprintBytes() const noexcept
{
    return static_cast<std::uint64_t>(m_payload.size());
}

} // namespace media::ffmpeg::graph
