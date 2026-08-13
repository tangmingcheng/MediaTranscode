#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <cstdint>
#include <span>

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaHevcSdpCodecDescriptionFactory final {
public:
    static ::media::Result<MediaHevcSdpCodecDescription> create(
        const AVCodecParameters& parameters,
        std::span<const std::uint8_t> accessUnitConfiguration);
};

} // namespace media::ffmpeg::graph
