#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaFfmpegAacAscDialectMaterializer final {
public:
    static ::media::Result<std::array<std::uint8_t, 2>> canonicalize(
        std::span<const std::uint8_t> bytes);

private:
    MediaFfmpegAacAscDialectMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
