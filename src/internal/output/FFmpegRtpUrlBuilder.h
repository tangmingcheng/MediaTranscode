#pragma once

#include <string>

namespace media::ffmpeg {

struct FFmpegRtpOutputConfig {
    std::string host;
    int rtpPort = 0;
    int rtcpPort = 0;
    int localRtpPort = 0;
    int localRtcpPort = 0;
    int packetSize = 1200;
    std::string sdpOutputPath;
};

class FFmpegRtpUrlBuilder {
public:
    static std::string build(const FFmpegRtpOutputConfig& config);
};

} // namespace media::ffmpeg
