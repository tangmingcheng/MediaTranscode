#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderPacketLayoutCapabilityProvider final {
public:
    static ::media::Result<MediaEncodedPacketLayout> probeOpenedContext(
        AVCodecContext& context);

    MediaEncoderPacketLayoutCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
