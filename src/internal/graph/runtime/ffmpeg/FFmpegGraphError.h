#pragma once

#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class FFmpegGraphError final {
public:
    static ::media::ErrorInfo fromCode(int code, std::string operation);
    static ::media::Status statusFromCode(int code, std::string operation);
    static std::string describe(int code);
};

} // namespace media::ffmpeg::graph
