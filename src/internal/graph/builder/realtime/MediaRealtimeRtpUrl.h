#pragma once

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

inline std::optional<std::size_t> parseRealtimeRtpUrlPort(const std::string& url)
{
    const std::size_t colon = url.rfind(':');
    if (colon == std::string::npos || colon + 1 >= url.size()) {
        return std::nullopt;
    }

    std::size_t port = 0;
    bool sawDigit = false;
    for (std::size_t index = colon + 1; index < url.size(); ++index) {
        const char ch = url[index];
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            break;
        }
        sawDigit = true;
        port = port * 10 + static_cast<std::size_t>(ch - '0');
    }
    if (!sawDigit || port == 0 || port > 65535) {
        return std::nullopt;
    }
    return port;
}

} // namespace media::ffmpeg::graph
