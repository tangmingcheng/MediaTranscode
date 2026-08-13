#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"
#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"

#include <optional>

namespace media::ffmpeg::graph {

enum class MediaTsPendingEmissionKind : std::uint8_t {
    Pat,
    Pmt,
    Pcr,
    AccessUnit
};

class MediaTsPendingEmission final {
public:
    MediaTsPendingEmission(
        MediaTsPacketCursor cursor,
        MediaRunningTime notBefore,
        MediaTsPendingEmissionKind kind,
        std::optional<MediaTsPreparedPacketClock> packetClock = std::nullopt,
        std::optional<MediaTsPreparedPcrClock> pcrClock = std::nullopt) noexcept;

    MediaTsPendingEmission(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission& operator=(MediaTsPendingEmission&&) noexcept = default;
    MediaTsPendingEmission(const MediaTsPendingEmission&) = delete;
    MediaTsPendingEmission& operator=(const MediaTsPendingEmission&) = delete;

    ::media::Result<MediaRunningTime> prepareNext(
        MediaTsPacketBatchWriter& writer,
        MediaTsDatagramEmissionSchedule& schedule,
        MediaRunningTime schedulingFloor);
    ::media::Result<MediaTsBatchWriteResult> emitPrepared(
        MediaTsPacketBatchWriter& writer,
        MediaTsDatagramEmissionSchedule& schedule);

    MediaRunningTime deadline() const noexcept;
    MediaTsPendingEmissionKind kind() const noexcept;
    bool finished() const noexcept;
    std::size_t packetsWritten() const noexcept;
    MediaRunningTime notBefore() const noexcept;
    std::optional<MediaTsPreparedPacketClock> takePacketClock() noexcept;
    std::optional<MediaTsPreparedPcrClock> takePcrClock() noexcept;

private:
    MediaTsPacketCursor m_cursor;
    MediaRunningTime m_notBefore;
    MediaTsPendingEmissionKind m_kind;
    std::optional<MediaTsPreparedPacketBatch> m_batch;
    std::optional<MediaTsPreparedDatagramEmission> m_emission;
    std::optional<MediaTsPreparedPacketClock> m_packetClock;
    std::optional<MediaTsPreparedPcrClock> m_pcrClock;
    std::size_t m_packetsWritten = 0;
};

} // namespace media::ffmpeg::graph
