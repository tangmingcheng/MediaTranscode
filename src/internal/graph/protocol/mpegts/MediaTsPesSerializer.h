#pragma once

#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaTsPesHeader final {
    std::array<std::uint8_t, 19> bytes{};
    std::size_t size = 0;
};

class MediaTsPesSerializer final {
public:
    static ::media::Result<MediaTsPesHeader> header(
        MediaScheduledStream stream,
        const MediaTsPacketClock& clock,
        std::size_t framedPayloadBytes);
};

} // namespace media::ffmpeg::graph
