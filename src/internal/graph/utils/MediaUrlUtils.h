#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRtpUrlEndpoint {
    std::string scheme;
    std::string host;
    uint16_t port = 0;
};

std::string redactUrlUserInfo(const std::string& url);
bool isUnsupportedRealtimeInputUrl(const std::string& url);
::media::Result<MediaRtpUrlEndpoint> parseRtpUdpUrlEndpoint(const std::string& url);

} // namespace media::ffmpeg::graph
