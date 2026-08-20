#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"
#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"
#include "internal/graph/diagnostics/MediaTsEmissionDiagnostics.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaTsPendingEmission final {
public:
    MediaTsPendingEmission(
        MediaTsPacketCursor cursor,
        MediaRunningTime notBefore,
        MediaTsPreparedPacketClock packetClock,
        std::size_t packetSizeBytes,
        std::size_t maximumPacketsPerDatagram) noexcept;

    MediaTsPendingEmission(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission& operator=(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission(const MediaTsPendingEmission&) = delete;
    MediaTsPendingEmission& operator=(const MediaTsPendingEmission&) = delete;

    ::media::Result<std::size_t> nextPayloadBytes() const;
    ::media::Status preparePayload(MediaTsPacketBatchWriter& writer);
    ::media::Result<std::size_t> preparedPayloadBytes() const;
    ::media::Result<MediaRunningTime> reservePrepared(
        MediaTsDatagramEmissionSchedule& schedule);
    ::media::Result<MediaTsBatchWriteResult> emitPrepared(
        MediaTsPacketBatchWriter& writer,
        MediaTsDatagramEmissionSchedule& schedule,
        MediaTsEmissionDiagnostics& diagnostics,
        const MediaMasterClock& masterClock,
        MediaRunningTime availableThrough,
        bool materializeScheduledBatch);

    MediaRunningTime deadline() const noexcept;
    bool finished() const noexcept;
    std::size_t pendingBytes() const noexcept;
    MediaRunningTime notBefore() const noexcept;
    std::optional<MediaTsPreparedPacketClock> takePacketClock() noexcept;

private:
    MediaTsPacketCursor m_cursor;
    MediaRunningTime m_notBefore;
    std::optional<MediaTsPreparedPacketBatch> m_batch;
    std::optional<MediaTsPreparedDatagramEmission> m_emission;
    std::optional<MediaTsPreparedPacketClock> m_packetClock;
    std::size_t m_packetSizeBytes;
    std::size_t m_maximumPacketsPerDatagram;
};

} // namespace media::ffmpeg::graph
