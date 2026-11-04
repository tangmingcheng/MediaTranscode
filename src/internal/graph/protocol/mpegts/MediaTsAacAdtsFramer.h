#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsAacAdtsFramer final {
public:
    // Storage bound only; actual frames must also fit the ADTS length field.
    static ::media::Result<std::size_t> maximumFramedPayloadBytes(
        const MediaTsAacAdtsPlan& plan, std::size_t rawPayloadBytes);

    static ::media::Result<std::span<const std::uint8_t>> frame(
        const MediaTsAacAdtsPlan& plan,
        std::span<const std::uint8_t> rawPayload,
        std::vector<std::uint8_t>& workspace);
};

} // namespace media::ffmpeg::graph
