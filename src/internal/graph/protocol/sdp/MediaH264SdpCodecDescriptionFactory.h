#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaH264SdpCodecDescriptionFactory final {
public:
    static ::media::Result<MediaH264SdpCodecDescription> create(
        const AVCodecParameters& parameters);
};

} // namespace media::ffmpeg::graph
