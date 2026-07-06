#pragma once

#include <string>

namespace media::ffmpeg::graph {

std::string redactUrlUserInfo(const std::string& url);
bool isUnsupportedRealtimeInputUrl(const std::string& url);

} // namespace media::ffmpeg::graph
