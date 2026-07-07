#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <algorithm>
#include <cctype>

namespace media::ffmpeg::graph {

std::string canonicalCodecName(std::string codec)
{
    std::transform(codec.begin(), codec.end(), codec.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (codec == "avc" || codec == "h.264") {
        return "h264";
    }
    if (codec == "h265" || codec == "h.265") {
        return "hevc";
    }
    return codec;
}

} // namespace media::ffmpeg::graph
