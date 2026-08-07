#pragma once

#include <string>

namespace media::ffmpeg::graph {

std::string lowercaseAscii(std::string value);
std::string canonicalCodecName(std::string codec);

} // namespace media::ffmpeg::graph
