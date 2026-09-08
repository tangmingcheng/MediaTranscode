#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

class MediaVideoSurfaceFootprint final {
public:
    static ::media::Result<std::uint64_t> logicalBytes(
        int width,
        int height,
        const std::string& pixelFormat);

private:
    MediaVideoSurfaceFootprint() = delete;
};

} // namespace media::ffmpeg::graph
