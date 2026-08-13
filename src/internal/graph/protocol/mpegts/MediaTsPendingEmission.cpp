#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsPendingEmission::MediaTsPendingEmission(
    MediaTsPacketCursor cursor,
    MediaRunningTime notBefore,
    MediaTsPendingEmissionKind kind,
    std::optional<MediaTsPreparedPacketClock> packetClock,
    std::optional<MediaTsPreparedPcrClock> pcrClock) noexcept
    : m_cursor(std::move(cursor))
    , m_notBefore(notBefore)
    , m_kind(kind)
    , m_packetClock(std::move(packetClock))
    , m_pcrClock(std::move(pcrClock))
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
    MediaTsDatagramEmissionSchedule& schedule)
{
    if (!m_batch || !m_emission) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS pending emission has no prepared datagram"));
    }
    const MediaRunningTime selectedDeadline = m_emission->deadline();
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
    if (m_packetsWritten >
        (std::numeric_limits<std::size_t>::max)() -
            written.value().packetsWritten) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending packet count overflow"));
    }
    m_packetsWritten += written.value().packetsWritten;
    return written;
}

MediaRunningTime MediaTsPendingEmission::deadline() const noexcept
{
    return m_emission
        ? m_emission->deadline()
        : m_notBefore;
}

MediaTsPendingEmissionKind MediaTsPendingEmission::kind() const noexcept
{
    return m_kind;
}

bool MediaTsPendingEmission::finished() const noexcept
{
    return m_cursor.finished() && !m_batch && !m_emission;
}

std::size_t MediaTsPendingEmission::packetsWritten() const noexcept
{
    return m_packetsWritten;
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

std::optional<MediaTsPreparedPcrClock>
MediaTsPendingEmission::takePcrClock() noexcept
{
    return std::move(m_pcrClock);
}

} // namespace media::ffmpeg::graph
