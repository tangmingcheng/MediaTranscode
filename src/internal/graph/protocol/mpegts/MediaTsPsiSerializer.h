#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsProgramTables final {
    std::vector<std::uint8_t> pat;
    std::vector<std::uint8_t> pmt;
};

class MediaTsPsiSerializer final {
public:
    static ::media::Result<MediaTsProgramTables> serialize(
        const MediaTsMuxPlan& plan);
};

} // namespace media::ffmpeg::graph
