#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaTsTimestampFieldSerializer final {
public:
    static ::media::Result<std::array<std::uint8_t, 5>> serialize(
        std::uint8_t prefix,
        std::uint64_t wireTimestamp);
};

} // namespace media::ffmpeg::graph
