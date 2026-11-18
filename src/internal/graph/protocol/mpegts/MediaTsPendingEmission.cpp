#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"
#include "internal/graph/protocol/mpegts/MediaTsPrefixStoragePreparation.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsPendingEmission::MediaTsPendingEmission(
    MediaTsPacketCursor cursor, MediaRunningTime notBefore, MediaRunningTime completionDeadline,
    MediaTsPreparedPacketClock packetClock, std::size_t packetSizeBytes,
    std::size_t maximumPacketsPerDatagram, MediaWireMediaBoundary finalBoundary) noexcept
    : m_cursor(std::move(cursor)),
      m_resource(m_cursor.storageResource()),
      m_geometry(m_cursor.storagePlan().protocolGeometry),
      m_notBefore(notBefore), m_completionDeadline(completionDeadline),
      m_packetClock(std::move(packetClock)),
      m_remainingPackets(m_cursor.remainingPacketCount()),
      m_packetSizeBytes(packetSizeBytes),
      m_maximumPacketsPerDatagram(maximumPacketsPerDatagram),
      m_finalBoundary(finalBoundary)
{
}

::media::Status MediaTsPendingEmission::waitForCommit(
    const std::shared_ptr<MediaNodeWakeup>& producerWakeup)
{
    if (!m_waitingCommit) return ::media::Status::success();
    std::optional<std::int64_t> committedAt;
    auto status = m_series->waitForCommit(producerWakeup, committedAt);
    if (status) {
        m_commitObservation.recordCompleted(committedAt);
        m_waitingCommit = false;
    } else if (status.error().code == ::media::ErrorCode::WouldBlock) {
        m_commitObservation.increment(m_commitObservation.wouldBlock);
    }
    return status;
}

::media::Result<std::optional<MediaTsUnpublishedTail>> MediaTsPendingEmission::unpublishedTail(
    const MediaTsDatagramEmissionSchedule& schedule) const
{
    using Result = ::media::Result<std::optional<MediaTsUnpublishedTail>>;
    if (!m_remainingPackets) return Result::success(std::nullopt);
    if (!m_packetSizeBytes || !m_maximumPacketsPerDatagram ||
        m_remainingPackets > (std::numeric_limits<std::size_t>::max)() / m_packetSizeBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS unpublished AU geometry is not representable"));
    const auto nextPackets = (std::min)(m_remainingPackets, m_maximumPacketsPerDatagram);
    auto preview = schedule.previewAccessUnit(nextPackets * m_packetSizeBytes);
    if (!preview) return Result::failure(preview.error());
    if (preview.value().accessUnitDeadline != m_completionDeadline)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS unpublished AU lost its original deadline"));
    return Result::success(MediaTsUnpublishedTail{preview.value().notBefore, m_completionDeadline,
        m_remainingPackets * m_packetSizeBytes,
        m_remainingPackets / m_maximumPacketsPerDatagram + (m_remainingPackets % m_maximumPacketsPerDatagram != 0)});
}

::media::Result<MediaBufferRef> MediaTsPendingEmission::materializePrefix(
    std::uint64_t generation, MediaTsDatagramEmissionSchedule& schedule,
    const MediaTsPrefixBatch& prefix)
{
    using Result = ::media::Result<MediaBufferRef>;
    if (!generation || m_waitingCommit || !m_packetSizeBytes ||
        !m_maximumPacketsPerDatagram || m_maximumPacketsPerDatagram >
            (std::numeric_limits<std::uint16_t>::max)() ||
        m_maximumPacketsPerDatagram > (std::numeric_limits<std::size_t>::max)() / m_packetSizeBytes ||
        m_remainingPackets > (std::numeric_limits<std::size_t>::max)() / m_packetSizeBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix identity or datagram geometry is invalid"));
    const auto maximumPayload = m_maximumPacketsPerDatagram * m_packetSizeBytes;
    if (!prefix.payloadBytes || prefix.payloadBytes > pendingBytes() ||
        prefix.payloadBytes % m_packetSizeBytes != 0 || !prefix.datagramCount ||
        (prefix.payloadBytes - 1) / maximumPayload + 1 != prefix.datagramCount ||
        (prefix.payloadBytes != pendingBytes() && prefix.payloadBytes % maximumPayload != 0))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS planned prefix differs from the pending contiguous payload"));
    const auto& window = prefix.window;
    // No allocation pressure may follow continuity reservation. The caller
    // has already checked its publication holder and previous commit.
    auto storage = MediaTsPrefixStoragePreparation::prepare(m_resource, m_geometry,
        static_cast<bool>(m_series));
    if (!storage) return Result::failure(storage.error());
    auto emission = schedule.prepareAccessUnit(prefix.payloadBytes);
    if (!emission) return Result::failure(emission.error());
    if (window.release != emission.value().deadline() ||
        window.accessUnitDeadline != emission.value().latestEmissionTime())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS prefix window differs from its schedule reservation"));
    // One planner-selected contiguous reservation is the publication/commit
    // quantum. Its unchanged internal limit protects imminent maintenance.
    if (!m_series) {
        auto created = MediaMpegTsProtocolDatagramBatchBuffer::create(
            generation, std::move(m_cursor),
            static_cast<std::uint16_t>(m_maximumPacketsPerDatagram),
            m_notBefore, window.release, window.accessUnitDeadline,
            m_finalBoundary, prefix.datagramCount, window.completionLimit, window.acceptedWorkObservation);
        if (!created) return Result::failure(created.error());
        m_series = std::move(created).value();
    } else {
        auto reserved = m_series->reserveNextPrefix(prefix.datagramCount, window,
            std::move(*storage.value().commitModel));
        if (!reserved) return Result::failure(reserved.error());
    }
    auto view = m_series->takePrefixView(prefix.datagramCount, std::move(storage.value().view));
    if (!view) return Result::failure(view.error());
    auto committed = schedule.commit(std::move(emission).value(), window.release);
    if (!committed) return Result::failure(committed.error());
    m_remainingPackets -= prefix.payloadBytes / m_packetSizeBytes;
    m_waitingCommit = true;
    return Result::success(std::move(view).value());
}

bool MediaTsPendingEmission::finished() const noexcept
{
    return m_series && m_remainingPackets == 0 && !m_waitingCommit;
}

std::size_t MediaTsPendingEmission::pendingBytes() const noexcept
{
    return !m_packetSizeBytes ||
        m_remainingPackets > (std::numeric_limits<std::size_t>::max)() / m_packetSizeBytes
        ? (std::numeric_limits<std::size_t>::max)()
        : m_remainingPackets * m_packetSizeBytes;
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
