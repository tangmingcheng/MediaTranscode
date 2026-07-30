#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Result<MediaTsPacketBatchWriter> MediaTsPacketBatchWriter::create(
    std::uint8_t maximumPacketsPerDatagram,
    std::unique_ptr<MediaTsDatagramSink> sink,
    std::unique_ptr<MediaTsPacketCommitter> committer)
{
    if (!sink || !committer || maximumPacketsPerDatagram < 1 ||
        maximumPacketsPerDatagram > 7) {
        return ::media::Result<MediaTsPacketBatchWriter>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS batch writer binding is invalid"));
    }
    return ::media::Result<MediaTsPacketBatchWriter>::success(
        MediaTsPacketBatchWriter(
            maximumPacketsPerDatagram, std::move(sink), std::move(committer)));
}

MediaTsPacketBatchWriter::MediaTsPacketBatchWriter(
    std::uint8_t maximumPacketsPerDatagram,
    std::unique_ptr<MediaTsDatagramSink> sink,
    std::unique_ptr<MediaTsPacketCommitter> committer)
    : m_maximumPacketsPerDatagram(maximumPacketsPerDatagram),
      m_sink(std::move(sink)),
      m_committer(std::move(committer))
{
}

MediaTsPacketBatchWriter::~MediaTsPacketBatchWriter()
{
    closeNoexcept();
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

::media::Result<std::size_t> MediaTsPacketBatchWriter::writeCursor(
    MediaTsPacketCursor& cursor,
    MediaRunningTime emitOnMaster)
{
    if (m_failure) {
        return ::media::Result<std::size_t>::failure(m_failure.value());
    }
    if (m_closed || !m_sink) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "MPEG-TS batch writer is closed"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    std::size_t committedPackets = 0;
    while (!cursor.finished()) {
        auto prepared = cursor.prepare(m_maximumPacketsPerDatagram);
        if (!prepared) {
            auto status = fail(prepared.error());
            return ::media::Result<std::size_t>::failure(status.error());
        }
        auto batch = std::move(prepared).value();
        const auto packets = batch.packets();
        if (packets.empty() || packets.size() > m_maximumPacketsPerDatagram) {
            auto status = fail(::media::ErrorInfo::invalidArgument(
                "MPEG-TS prepared batch is outside the planned datagram limit"));
            return ::media::Result<std::size_t>::failure(status.error());
        }
        const std::size_t expected = packets.size() * std::size_t{188};
        auto output = m_datagram.begin();
        for (const auto& packet : packets) {
            output = std::copy(packet.begin(), packet.end(), output);
        }
        auto written = m_sink->write(
            std::span<const std::uint8_t>(m_datagram.data(), expected),
            emitOnMaster);
        if (!written) {
            auto status = fail(written.error());
            return ::media::Result<std::size_t>::failure(status.error());
        }
        if (written.value() != expected) {
            auto status = fail(::media::ErrorInfo::ioFailure(
                "MPEG-TS datagram sink returned a short write"));
            return ::media::Result<std::size_t>::failure(status.error());
        }
        auto committed = m_committer->commit(
            cursor, batch.takeCommitToken());
        if (!committed) {
            auto status = fail(committed.error());
            return ::media::Result<std::size_t>::failure(status.error());
        }
        committedPackets += packets.size();
    }
    return ::media::Result<std::size_t>::success(committedPackets);
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

void MediaTsPacketBatchWriter::closeNoexcept() noexcept
{
    if (m_closed || !m_sink) return;
    auto status = m_sink->close();
    m_closed = true;
    if (!status && !m_failure) m_failure = status.error();
}

void MediaTsPacketBatchWriter::abort() noexcept
{
    closeNoexcept();
}

} // namespace media::ffmpeg::graph
