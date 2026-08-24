#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo emissionIntervalError(
    const char* stage,
    MediaRunningTime deadline,
    MediaRunningTime observed,
    MediaRunningTime latest)
{
    std::ostringstream message;
    message << "MPEG-TS access-unit emission is outside its canonical interval"
            << " stage=" << stage
            << " deadline_ns=" << deadline.nanoseconds()
            << " observed_ns=" << observed.nanoseconds()
            << " latest_ns=" << latest.nanoseconds();
    return ::media::ErrorInfo::invalidArgument(message.str());
}

} // namespace

MediaTsPendingEmission::MediaTsPendingEmission(
    MediaTsPacketCursor cursor,
    MediaRunningTime notBefore,
    MediaTsPreparedPacketClock packetClock,
    std::size_t packetSizeBytes,
    std::size_t maximumPacketsPerDatagram) noexcept
    : m_cursor(std::move(cursor))
    , m_notBefore(notBefore)
    , m_packetClock(std::move(packetClock))
    , m_packetSizeBytes(packetSizeBytes)
    , m_maximumPacketsPerDatagram(maximumPacketsPerDatagram)
{
}

::media::Result<std::size_t>
MediaTsPendingEmission::nextPayloadBytes() const
{
    if (m_materialized || m_cursor.finished() || m_batch || m_emission ||
        m_packetSizeBytes == 0 || m_maximumPacketsPerDatagram == 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending emission has no next payload geometry"));
    }
    const std::size_t packets = (std::min)(
        m_cursor.remainingPacketCount(), m_maximumPacketsPerDatagram);
    if (packets == 0 ||
        packets > (std::numeric_limits<std::size_t>::max)() /
            m_packetSizeBytes) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending next payload size is not representable"));
    }
    return ::media::Result<std::size_t>::success(
        packets * m_packetSizeBytes);
}

::media::Result<MediaBufferRef>
MediaTsPendingEmission::materializeProtocolBatch(
    std::uint64_t generation,
    MediaTsDatagramEmissionSchedule& schedule)
{
    if (m_materialized || m_batch || m_emission || generation == 0 ||
        m_cursor.finished() || m_packetSizeBytes == 0 ||
        m_maximumPacketsPerDatagram == 0 ||
        m_cursor.remainingPacketCount() >
            (std::numeric_limits<std::size_t>::max)() / m_packetSizeBytes) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending emission cannot materialize its protocol transaction"));
    }
    const std::size_t payloadBytes =
        m_cursor.remainingPacketCount() * m_packetSizeBytes;
    auto emission = schedule.prepareAccessUnit(payloadBytes);
    if (!emission) {
        return ::media::Result<MediaBufferRef>::failure(emission.error());
    }
    const MediaRunningTime release = emission.value().deadline();
    const MediaRunningTime deadline = emission.value().latestEmissionTime();
    auto batch = MediaMpegTsProtocolDatagramBatchBuffer::create(
        generation, std::move(m_cursor),
        static_cast<std::uint8_t>(m_maximumPacketsPerDatagram),
        m_notBefore, release, deadline);
    if (!batch) return ::media::Result<MediaBufferRef>::failure(batch.error());
    auto committed = schedule.commit(
        std::move(emission).value(), release);
    if (!committed) {
        return ::media::Result<MediaBufferRef>::failure(committed.error());
    }
    m_materialized = true;
    return ::media::Result<MediaBufferRef>::success(
        std::move(batch).value());
}

::media::Status MediaTsPendingEmission::preparePayload(
    MediaTsPacketBatchWriter& writer)
{
    if (m_cursor.finished() || m_batch || m_emission) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending emission cannot prepare its current state"));
    }
    auto batch = writer.prepareNext(m_cursor);
    if (!batch) {
        return ::media::Status::failure(batch.error());
    }
    m_batch = std::move(batch).value();
    return ::media::Status::success();
}

::media::Result<std::size_t>
MediaTsPendingEmission::preparedPayloadBytes() const
{
    if (!m_batch || m_emission) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS pending emission has no unreserved payload"));
    }
    const std::size_t packetCount = m_batch->packets().size();
    if (packetCount == 0 || m_packetSizeBytes == 0 ||
        packetCount > (std::numeric_limits<std::size_t>::max)() /
            m_packetSizeBytes) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending batch size is not representable"));
    }
    return ::media::Result<std::size_t>::success(
        packetCount * m_packetSizeBytes);
}

::media::Result<MediaRunningTime> MediaTsPendingEmission::reservePrepared(
    MediaTsDatagramEmissionSchedule& schedule)
{
    auto payloadBytes = preparedPayloadBytes();
    if (!payloadBytes) {
        return ::media::Result<MediaRunningTime>::failure(
            payloadBytes.error());
    }
    if (schedule.plan().packetSizeBytes() != m_packetSizeBytes) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS pending packet size conflicts with its schedule"));
    }
    auto emission = schedule.prepareAccessUnit(
        payloadBytes.value());
    if (!emission) {
        return ::media::Result<MediaRunningTime>::failure(emission.error());
    }
    const MediaRunningTime deadline = emission.value().deadline();
    m_emission = std::move(emission).value();
    return ::media::Result<MediaRunningTime>::success(deadline);
}

::media::Result<MediaTsBatchWriteResult>
MediaTsPendingEmission::emitPrepared(
    MediaTsPacketBatchWriter& writer,
    MediaTsDatagramEmissionSchedule& schedule,
    MediaTsEmissionDiagnostics& diagnostics,
    const MediaMasterClock& masterClock,
    MediaRunningTime availableThrough,
    bool materializeScheduledBatch)
{
    if (!m_batch || !m_emission) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS pending emission has no prepared datagram"));
    }
    const MediaRunningTime selectedDeadline = m_emission->deadline();
    if (!materializeScheduledBatch &&
        (availableThrough < selectedDeadline ||
         availableThrough > m_emission->latestEmissionTime())) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            emissionIntervalError(
                "before_write", selectedDeadline, availableThrough,
                m_emission->latestEmissionTime()));
    }
    const MediaRunningTime plannedWait = m_emission->plannedWait();
    const std::size_t wireBytes = m_emission->wireBytes();
    auto window = MediaTsDatagramEnqueueWindow::create(
        selectedDeadline, m_emission->latestEmissionTime(),
        m_emission->serviceDuration());
    if (!window) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            window.error());
    }
    auto written = writer.writeNext(
        m_cursor, std::move(*m_batch), window.value());
    if (!written) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            written.error());
    }
    auto actualEmission = materializeScheduledBatch
        ? ::media::Result<MediaRunningTime>::success(selectedDeadline)
        : masterClock.now();
    if (!actualEmission || actualEmission.value() < selectedDeadline ||
        actualEmission.value() > m_emission->latestEmissionTime()) {
        return ::media::Result<MediaTsBatchWriteResult>::failure(
            actualEmission
                ? emissionIntervalError(
                      "after_write", selectedDeadline,
                      actualEmission.value(),
                      m_emission->latestEmissionTime())
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
    if (!materializeScheduledBatch) {
        diagnostics.recordCommittedDatagram(
            wireBytes, plannedWait, selectedDeadline, actualEmission.value());
    }
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
    return m_materialized ||
        (m_cursor.finished() && !m_batch && !m_emission);
}

std::size_t MediaTsPendingEmission::pendingBytes() const noexcept
{
    const std::size_t packets = m_materialized
        ? 0 : m_cursor.remainingPacketCount();
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
