#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaAacLatmSdpCodecDescriptionFactory final {
public:
    static ::media::Result<MediaAacLatmSdpCodecDescription> create(
        const AVCodecParameters& parameters);
};

} // namespace media::ffmpeg::graph
