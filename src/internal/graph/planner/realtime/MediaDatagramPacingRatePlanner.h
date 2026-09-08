#pragma once

#include "internal/graph/planner/realtime/MediaWireTrafficEnvelope.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaDatagramPacingRatePlanner final {
public:
    static ::media::Result<std::uint64_t> requiredWireBytesPerSecond(
        const MediaWireTrafficEnvelope& wire,
        MediaRunningTime maximumResidence);

    MediaDatagramPacingRatePlanner() = delete;
};

} // namespace media::ffmpeg::graph
