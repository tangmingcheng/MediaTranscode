#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaHardwareBackendRequest {
    Auto,
    RKMPP
};

inline const char* mediaHardwareBackendRequestName(MediaHardwareBackendRequest request) noexcept
{
    switch (request) {
    case MediaHardwareBackendRequest::Auto: return "auto";
    case MediaHardwareBackendRequest::RKMPP: return "rkmpp";
    }
    return "unknown";
}

inline bool parseMediaHardwareBackendRequest(
    std::string_view text,
    MediaHardwareBackendRequest& request) noexcept
{
    if (text.empty() || text == "auto") {
        request = MediaHardwareBackendRequest::Auto;
        return true;
    }
    if (text == "rkmpp") {
        request = MediaHardwareBackendRequest::RKMPP;
        return true;
    }
    return false;
}

} // namespace media::ffmpeg::graph
