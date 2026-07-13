#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaTsEvidenceCheckpoint final {
    std::uint64_t byteOffset = 0;
    MediaTsProgramInventorySnapshot inventory;
    std::optional<std::int64_t> pcr27Mhz;
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
