#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRtpVideoParameterSetInfo final {
    MediaSize codedSize;
};

class MediaRtpVideoParameterSetValidator final {
public:
    static ::media::Result<MediaRtpVideoParameterSetInfo> inspect(
        const std::string& codecName,
        const MediaRtpVideoSignalingFacts& facts);
    static ::media::Status validate(
        const std::string& codecName,
        const MediaRtpVideoSignalingFacts& facts);

private:
    MediaRtpVideoParameterSetValidator() = delete;
};

} // namespace media::ffmpeg::graph
