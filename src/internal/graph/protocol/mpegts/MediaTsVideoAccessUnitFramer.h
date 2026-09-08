#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsVideoAccessUnitFramer final {
public:
    static ::media::Result<std::span<const std::uint8_t>> frame(
        const MediaTsMuxPlan& plan,
        const MediaTsMaterializedVideoConfig& config,
        std::span<const std::uint8_t> payload,
        bool randomAccess,
        std::vector<std::uint8_t>& workspace);
};

} // namespace media::ffmpeg::graph
