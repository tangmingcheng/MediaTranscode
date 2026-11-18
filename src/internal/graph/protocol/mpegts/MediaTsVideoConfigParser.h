#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaTsVideoConfigParser final {
public:
    static ::media::Result<MediaTsMaterializedVideoConfig> parse(
        const MediaTsVideoElementaryStreamContract& contract,
        std::span<const std::uint8_t> bytes);
};

} // namespace media::ffmpeg::graph
