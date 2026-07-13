#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsRawPcrEvidence final {
    std::uint64_t byteOffset = 0;
    std::uint16_t pid = 0;
    std::uint64_t pcr27Mhz = 0;
    bool discontinuity = false;

    bool operator==(const MediaTsRawPcrEvidence&) const = default;
};

struct MediaTsEvidenceCheckpoint final {
    std::uint64_t byteOffset = 0;
    MediaTsProgramInventorySnapshot inventory;
    std::optional<MediaTsRawPcrEvidence> pcrObservation;
    bool discontinuity = false;
    std::uint64_t generation = 0;
};

class MediaTsEvidenceTimeline final {
public:
    static ::media::Result<MediaTsEvidenceTimeline> create(
        std::size_t capacity,
        std::uint64_t maximumPositionRegressionBytes);

    ::media::Status append(MediaTsEvidenceCheckpoint checkpoint);
    ::media::Result<MediaTsEvidenceCheckpoint> atOrBefore(
        std::uint64_t packetPosition) const;
    ::media::Status observePacketPosition(std::uint64_t packetPosition);
    ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> snapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const;

private:
    MediaTsEvidenceTimeline(std::size_t capacity,
                            std::uint64_t maximumPositionRegressionBytes) noexcept;
    void evictSafeCheckpoints();

    std::size_t m_capacity;
    std::uint64_t m_maximumPositionRegressionBytes;
    std::optional<std::uint64_t> m_highWatermark;
    std::deque<MediaTsEvidenceCheckpoint> m_checkpoints;
};

} // namespace media::ffmpeg::graph
