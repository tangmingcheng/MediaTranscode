#pragma once

#include <sstream>
#include <string>

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg {

inline std::string errorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buffer, sizeof(buffer));

    std::ostringstream oss;
    oss << buffer << " (" << err << ")";
    return oss.str();
}

} // namespace media::ffmpeg
