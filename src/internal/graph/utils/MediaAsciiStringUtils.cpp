#include "internal/graph/utils/MediaAsciiStringUtils.h"

#include <algorithm>
#include <cctype>

namespace media::ffmpeg::graph {

std::string lowercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace media::ffmpeg::graph
