#include "internal/output/builders/rtp/FFmpegRtpUrlBuilder.h"

#include <sstream>
#include <vector>

namespace media::ffmpeg {

namespace {

void addIntOption(std::vector<std::string>& options,
                  const char* name,
                  int value,
                  bool allowZero = false)
{
    if (value <= 0 && !allowZero) {
        return;
    }

    std::ostringstream oss;
    oss << name << '=' << value;
    options.emplace_back(oss.str());
}

} // namespace

std::string FFmpegRtpUrlBuilder::build(const FFmpegRtpOutputConfig& config)
{
    std::ostringstream url;
    url << "rtp://" << config.host << ':' << config.rtpPort;

    std::vector<std::string> options;
    addIntOption(options, "rtcpport", config.rtcpPort);
    addIntOption(options, "localrtpport", config.localRtpPort);
    addIntOption(options, "localrtcpport", config.localRtcpPort);
    addIntOption(options, "pkt_size", config.packetSize);

    for (std::size_t i = 0; i < options.size(); ++i) {
        url << (i == 0 ? '?' : '&') << options[i];
    }

    return url.str();
}

} // namespace media::ffmpeg
