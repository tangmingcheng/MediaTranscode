#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaRtpIngressCapabilityMaterializer final {
public:
    static ::media::Result<MediaRtpIngressCapability> materialize(
        std::size_t effectiveSocketReceivePayloadBytes);

private:
    MediaRtpIngressCapabilityMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
