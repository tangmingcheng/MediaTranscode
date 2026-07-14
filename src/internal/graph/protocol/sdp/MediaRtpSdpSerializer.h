#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaRtpSdpSerializer final {
public:
    static ::media::Result<std::string> serialize(
        const MediaRtpSdpDescription& description);
};

} // namespace media::ffmpeg::graph
