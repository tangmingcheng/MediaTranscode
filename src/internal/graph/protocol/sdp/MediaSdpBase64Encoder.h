#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

class MediaSdpBase64Encoder final {
public:
    static ::media::Result<std::string> encode(
        std::span<const std::uint8_t> bytes);
};

} // namespace media::ffmpeg::graph
