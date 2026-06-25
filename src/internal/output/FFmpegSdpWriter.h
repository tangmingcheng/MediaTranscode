#pragma once

#include "media_transcode/Result.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegSdpWriter {
public:
    static Status save(AVFormatContext* formatContext,
                       const std::string& path);
};

} // namespace media::ffmpeg
