#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsPendingEmission::MediaTsPendingEmission(
    MediaTsPacketCursor cursor,
    MediaRunningTime notBefore,
    MediaTsPreparedPacketClock packetClock,
    std::size_t packetSizeBytes) noexcept
    : m_cursor(std::move(cursor))
    , m_notBefore(notBefore)
    , m_packetClock(std::move(packetClock))
    , m_packetSizeBytes(packetSizeBytes)
{
}

::media::Result<MediaRunningTime> MediaTsPendingEmission::prepareNext(
    MediaTsPacketBatchWriter& writer,
    MediaTsDatagramEmissionSchedule& schedule)
{
    if (m_cursor.finished() || m_batch || m_emission) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending emission cannot prepare its current state"));
    }
    auto batch = writer.prepareNext(m_cursor);
    if (!batch) {
        return ::media::Result<MediaRunningTime>::failure(batch.error());
    }
    const std::size_t packetCount = batch.value().packets().size();
    if (packetCount == 0 || m_packetSizeBytes == 0 ||
        schedule.plan().packetSizeBytes() != m_packetSizeBytes ||
        packetCount > (std::numeric_limits<std::size_t>::max)() /
            m_packetSizeBytes) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending batch size is not representable"));
    }
    auto emission = schedule.prepareAccessUnit(
        packetCount * m_packetSizeBytes);
    if (!emission) {
        return ::media::Result<MediaRunningTime>::failure(emission.error());
    }
    const MediaRunningTime deadline = emission.value().deadline();
    m_batch = std::move(batch).value();
    m_emission = std::move(emission).value();
    return ::media::Result<MediaRunningTime>::success(deadline);
}

::media::Result<MediaTsBatchWriteResult>
MediaTsPendingEmission::emitPrepared(
    MediaTsPacketBatchWriter& writer,
    MediaTsDatagramEmissionSchedule& schedule,
    MediaTsEmissionDiagnostics& diagnostics,
    const MediaMasterClock& masterClock,
    MediaRunningTime availableThrough)
{
    if (!m_batch || !m_emission) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS pending emission has no prepared datagram"));
    }
    const MediaRunningTime selectedDeadline = m_emission->deadline();
    if (availableThrough < selectedDeadline ||
        availableThrough > m_emission->latestEmissionTime()) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit emission is outside its canonical interval"));
    }
    const MediaRunningTime plannedWait = m_emission->plannedWait();
    const std::size_t wireBytes = m_emission->wireBytes();
    auto written = writer.writeNext(
        m_cursor, std::move(*m_batch), selectedDeadline);
    if (!written) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            written.error());
    }
    auto actualEmission = masterClock.now();
    if (!actualEmission || actualEmission.value() < selectedDeadline ||
        actualEmission.value() > m_emission->latestEmissionTime()) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            actualEmission
                ? ::media::ErrorInfo::invalidArgument(
                      "MPEG-TS access-unit write completed outside its canonical interval")
                : actualEmission.error());
    }
    auto committed = schedule.commit(
        std::move(*m_emission), actualEmission.value());
    if (!committed) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            committed.error());
    }
    m_batch.reset();
    m_emission.reset();
    diagnostics.recordCommittedDatagram(
        wireBytes, plannedWait, selectedDeadline, actualEmission.value());
    diagnostics.recordPendingBytes(pendingBytes());
    return written;
}

MediaRunningTime MediaTsPendingEmission::deadline() const noexcept
{
    return m_emission
        ? m_emission->deadline()
        : m_notBefore;
}

bool MediaTsPendingEmission::finished() const noexcept
{
    return m_cursor.finished() && !m_batch && !m_emission;
}

std::size_t MediaTsPendingEmission::pendingBytes() const noexcept
{
    const std::size_t packets = m_cursor.remainingPacketCount();
    return m_packetSizeBytes == 0 ||
           packets > (std::numeric_limits<std::size_t>::max)() /
               m_packetSizeBytes
        ? (std::numeric_limits<std::size_t>::max)()
        : packets * m_packetSizeBytes;
}

MediaRunningTime MediaTsPendingEmission::notBefore() const noexcept
{
    return m_notBefore;
}

std::optional<MediaTsPreparedPacketClock>
MediaTsPendingEmission::takePacketClock() noexcept
{
    return std::move(m_packetClock);
}

} // namespace media::ffmpeg::graph
