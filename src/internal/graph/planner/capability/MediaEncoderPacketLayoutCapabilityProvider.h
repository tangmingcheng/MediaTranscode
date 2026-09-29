#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include <optional>

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderPacketLayoutCapabilityProvider final {
public:
    static ::media::Result<MediaEncodedPacketLayout> probeOpenedContext(
        AVCodecContext& context,
        std::optional<MediaVideoColorRangeFact>& effectiveColorRange);

    MediaEncoderPacketLayoutCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
