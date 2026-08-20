#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <cstdint>
#include <span>

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaH264SdpCodecDescriptionFactory final {
public:
    static ::media::Result<MediaH264SdpCodecDescription> create(
        const AVCodecParameters& parameters,
        std::span<const std::uint8_t> codecConfigurationAccessUnit = {});
};

} // namespace media::ffmpeg::graph
