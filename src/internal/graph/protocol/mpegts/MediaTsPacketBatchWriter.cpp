#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"

#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {

::media::Result<MediaTsPacketBatchWriter> MediaTsPacketBatchWriter::create(
    std::uint16_t maximumPacketsPerDatagram,
    std::unique_ptr<MediaTsDatagramSink> sink,
    std::unique_ptr<MediaTsPacketCommitter> committer)
{
    if (!sink || !committer || maximumPacketsPerDatagram < 1) {
        return ::media::Result<MediaTsPacketBatchWriter>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS batch writer binding is invalid"));
    }
    try {
        std::vector<std::uint8_t> datagram(
            static_cast<std::size_t>(maximumPacketsPerDatagram) * 188U);
        return ::media::Result<MediaTsPacketBatchWriter>::success(
            MediaTsPacketBatchWriter(
                maximumPacketsPerDatagram, std::move(sink),
                std::move(committer), std::move(datagram)));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaTsPacketBatchWriter>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MPEG-TS batch writer Datagram storage"));
    }
}

MediaTsPacketBatchWriter::MediaTsPacketBatchWriter(
    std::uint16_t maximumPacketsPerDatagram,
    std::unique_ptr<MediaTsDatagramSink> sink,
    std::unique_ptr<MediaTsPacketCommitter> committer,
    std::vector<std::uint8_t> datagram)
    : m_maximumPacketsPerDatagram(maximumPacketsPerDatagram),
      m_sink(std::move(sink)),
      m_committer(std::move(committer)),
      m_datagram(std::move(datagram))
{
}

MediaTsPacketBatchWriter::~MediaTsPacketBatchWriter()
{
    abort();
}

::media::Status MediaTsPacketBatchWriter::firstFailure() const
{
    return ::media::Status::failure(*m_failure);
}

::media::Status MediaTsPacketBatchWriter::fail(::media::ErrorInfo error)
{
    if (!m_failure) m_failure = std::move(error);
    return firstFailure();
}

::media::Result<MediaTsPreparedPacketBatch>
MediaTsPacketBatchWriter::prepareNext(MediaTsPacketCursor& cursor)
{
    if (m_failure) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            m_failure.value());
    }
    if (m_closed || cursor.finished()) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS batch writer cannot prepare a closed or finished cursor"));
    }
    return cursor.prepare(m_maximumPacketsPerDatagram);
}

::media::Result<MediaTsBatchWriteResult>
MediaTsPacketBatchWriter::writeNext(
    MediaTsPacketCursor& cursor,
    const MediaTsDatagramEnqueueWindow& enqueueWindow)
{
    auto prepared = prepareNext(cursor);
    if (!prepared) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            prepared.error());
    }
    return writeNext(
        cursor, std::move(prepared).value(), enqueueWindow);
}

::media::Result<MediaTsBatchWriteResult>
MediaTsPacketBatchWriter::writeNext(
    MediaTsPacketCursor& cursor,
    MediaTsPreparedPacketBatch&& batch,
    const MediaTsDatagramEnqueueWindow& enqueueWindow)
{
    if (m_failure) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            m_failure.value());
    }
    if (m_closed || !m_sink) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "MPEG-TS batch writer is closed"));
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            status.error());
    }
    const auto packets = batch.packets();
    if (packets.empty() || packets.size() > m_maximumPacketsPerDatagram) {
        auto status = fail(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prepared batch is outside the planned datagram limit"));
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            status.error());
    }
    const std::size_t expected = packets.size() * std::size_t{188};
    auto output = m_datagram.begin();
    for (const auto& packet : packets) {
        output = std::copy(packet.begin(), packet.end(), output);
    }
    auto written = m_sink->write(
        std::span<const std::uint8_t>(m_datagram.data(), expected),
        enqueueWindow);
    if (!written) {
        auto status = fail(written.error());
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            status.error());
    }
    if (written.value() != expected) {
        auto status = fail(::media::ErrorInfo::ioFailure(
            "MPEG-TS datagram sink returned a short write"));
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            status.error());
    }
    auto committed = m_committer->commit(
        cursor, batch.takeCommitToken());
    if (!committed) {
        auto status = fail(committed.error());
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            status.error());
    }
    return ::media::Result<MediaTsBatchWriteResult>::success(
        MediaTsBatchWriteResult{
            packets.size(), expected, cursor.finished()});
}

::media::Status MediaTsPacketBatchWriter::finish()
{
    if (m_closed) {
        return m_failure ? firstFailure() : ::media::Status::success();
    }
    auto status = m_sink->flush();
    if (!status) fail(status.error());
    auto closeStatus = m_sink->close();
    m_closed = true;
    if (!closeStatus) fail(closeStatus.error());
    return m_failure ? firstFailure() : ::media::Status::success();
}

void MediaTsPacketBatchWriter::abort() noexcept
{
    if (m_closed || !m_sink) return;
    m_sink->abort();
    m_closed = true;
}

} // namespace media::ffmpeg::graph
