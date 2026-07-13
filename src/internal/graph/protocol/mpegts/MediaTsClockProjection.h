#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsClockProjectionCheckpoint final {
    std::uint64_t byteOffset = 0;
    std::optional<MediaTsPcrCalibration> calibration;
    std::uint64_t generation = 0;
};

class MediaTsClockProjection final {
public:
    static ::media::Result<MediaTsClockProjection> create(
        MediaTsProgramClockPolicy policy,
        std::size_t capacity,
        std::uint64_t maximumPositionRegressionBytes);

    ::media::Status replay(const std::vector<MediaTsEvidenceCheckpoint>& evidence);
    ::media::Result<MediaTsClockProjectionCheckpoint> atOrBefore(
        std::uint64_t packetPosition) const;
    std::optional<std::uint64_t> lastReplayedOffset() const noexcept;
    ::media::Status observePacketPosition(std::uint64_t packetPosition);

private:
    MediaTsClockProjection(MediaTsProgramClockPolicy policy,
                           std::size_t capacity,
                           std::uint64_t maximumPositionRegressionBytes,
                           MediaTsProgramClockTracker tracker) noexcept;
    ::media::Status replayOne(const MediaTsEvidenceCheckpoint& evidence);
    ::media::Status validateInventory(const MediaTsProgramInventorySnapshot& inventory) const;

    MediaTsProgramClockPolicy m_policy;
    std::size_t m_capacity;
    std::uint64_t m_maximumPositionRegressionBytes;
    MediaTsProgramClockTracker m_tracker;
    std::vector<MediaTsClockProjectionCheckpoint> m_checkpoints;
    std::optional<std::uint64_t> m_lastReplayedOffset;
    std::optional<std::uint64_t> m_packetPositionHighWatermark;
};

} // namespace media::ffmpeg::graph
