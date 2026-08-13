#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsPendingEmission::MediaTsPendingEmission(
    MediaTsPacketCursor cursor,
    MediaRunningTime notBefore,
    MediaTsPreparedPacketClock packetClock) noexcept
    : m_cursor(std::move(cursor))
    , m_notBefore(notBefore)
    , m_packetClock(std::move(packetClock))
{
}

::media::Result<MediaRunningTime> MediaTsPendingEmission::prepareNext(
    MediaTsPacketBatchWriter& writer,
    MediaTsDatagramEmissionSchedule& schedule,
    MediaRunningTime schedulingFloor)
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
    if (packetCount == 0 ||
        packetCount > (std::numeric_limits<std::size_t>::max)() / 188) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending batch size is not representable"));
    }
    auto emission = schedule.prepare(
        packetCount * 188,
        (std::max)(m_notBefore, schedulingFloor));
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
    MediaRunningTime actualEmission)
{
    if (!m_batch || !m_emission) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS pending emission has no prepared datagram"));
    }
    const MediaRunningTime selectedDeadline = m_emission->deadline();
    auto actualLateness = actualEmission.checkedSubtract(selectedDeadline);
    if (!actualLateness) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            actualLateness.error());
    }
    if (actualLateness.value() > schedule.plan().maximumLateness()) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending datagram exceeded maximum emission lateness"));
    }
    const MediaRunningTime plannedWait = m_emission->plannedWait();
    const std::size_t wireBytes = m_emission->wireBytes();
    auto written = writer.writeNext(
        m_cursor, std::move(*m_batch), selectedDeadline);
    if (!written) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            written.error());
    }
    auto committed = schedule.commit(std::move(*m_emission));
    if (!committed) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            committed.error());
    }
    m_batch.reset();
    m_emission.reset();
    diagnostics.recordCommittedDatagram(
        wireBytes, plannedWait, selectedDeadline, actualEmission);
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
    return packets > (std::numeric_limits<std::size_t>::max)() / 188
        ? (std::numeric_limits<std::size_t>::max)()
        : packets * 188;
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
