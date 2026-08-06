#pragma once

#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaRtpVideoParameterSetValidator final {
public:
    static ::media::Status validate(
        const std::string& codecName,
        const MediaRtpVideoSignalingFacts& facts);

private:
    MediaRtpVideoParameterSetValidator() = delete;
};

} // namespace media::ffmpeg::graph
