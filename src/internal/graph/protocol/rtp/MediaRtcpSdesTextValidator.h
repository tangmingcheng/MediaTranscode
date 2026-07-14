#pragma once

#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

class MediaRtcpSdesTextValidator final {
public:
    static ::media::Status validateCname(std::string_view cname);

private:
    MediaRtcpSdesTextValidator() = delete;
};

} // namespace media::ffmpeg::graph
