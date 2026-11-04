#pragma once
#include "internal/graph/diagnostics/MediaTsPrefixCommitObservation.h"

#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"
#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/planner/realtime/MediaTsPrefixPublicationPlanner.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaTsPendingEmission final {
public:
    MediaTsPendingEmission(
        MediaTsPacketCursor cursor,
        MediaRunningTime notBefore,
        MediaRunningTime completionDeadline,
        MediaTsPreparedPacketClock packetClock,
        std::size_t packetSizeBytes,
        std::size_t maximumPacketsPerDatagram,
        MediaWireMediaBoundary finalBoundary) noexcept;

    MediaTsPendingEmission(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission& operator=(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission(const MediaTsPendingEmission&) = delete;
    MediaTsPendingEmission& operator=(const MediaTsPendingEmission&) = delete;

    ::media::Status waitForCommit(const std::shared_ptr<MediaNodeWakeup>& producerWakeup);
    ::media::Result<std::optional<MediaTsUnpublishedTail>> unpublishedTail(
        const MediaTsDatagramEmissionSchedule& schedule) const;
    ::media::Result<MediaBufferRef> materializePrefix(
        std::uint64_t generation, MediaTsDatagramEmissionSchedule& schedule,
        const MediaTsPrefixBatch& prefix);

    void recordPublished() noexcept { m_commitObservation.recordPublished(); }
    const MediaTsPrefixCommitObservation& commitObservation() const noexcept { return m_commitObservation; }
    bool finished() const noexcept;
    std::size_t pendingBytes() const noexcept;
    MediaRunningTime notBefore() const noexcept;
    MediaRunningTime completionDeadline() const noexcept { return m_completionDeadline; }
    std::optional<MediaTsPreparedPacketClock> takePacketClock() noexcept;

private:
    MediaTsPacketCursor m_cursor;
    std::shared_ptr<MediaNetworkMemoryResource> m_resource;
    MediaTsProtocolSeriesStorageGeometry m_geometry;
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> m_series;
    MediaRunningTime m_notBefore;
    MediaRunningTime m_completionDeadline;
    std::optional<MediaTsPreparedPacketClock> m_packetClock;
    std::size_t m_remainingPackets;
    std::size_t m_packetSizeBytes;
    std::size_t m_maximumPacketsPerDatagram;
    MediaWireMediaBoundary m_finalBoundary;
    bool m_waitingCommit = false;
    MediaTsPrefixCommitObservation m_commitObservation;
};

} // namespace media::ffmpeg::graph
