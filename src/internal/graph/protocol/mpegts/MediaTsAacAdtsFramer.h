#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsAacAdtsFramer final {
public:
    static ::media::Result<std::span<const std::uint8_t>> frame(
        const MediaTsAacAdtsPlan& plan,
        std::span<const std::uint8_t> rawPayload,
        std::vector<std::uint8_t>& workspace);
};

} // namespace media::ffmpeg::graph
