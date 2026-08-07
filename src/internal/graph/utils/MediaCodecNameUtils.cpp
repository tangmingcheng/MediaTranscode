#include "internal/graph/utils/MediaCodecNameUtils.h"

#include "internal/graph/utils/MediaAsciiStringUtils.h"

#include <utility>

namespace media::ffmpeg::graph {

std::string canonicalCodecName(std::string codec)
{
    codec = lowercaseAscii(std::move(codec));
    if (codec == "avc" || codec == "h.264") {
        return "h264";
    }
    if (codec == "h265" || codec == "h.265") {
        return "hevc";
    }
    if (codec == "mp4a" || codec == "mpeg4aac" || codec == "aac_lc") {
        return "aac";
    }
    if (codec == "libopus") {
        return "opus";
    }
    return codec;
}

} // namespace media::ffmpeg::graph
