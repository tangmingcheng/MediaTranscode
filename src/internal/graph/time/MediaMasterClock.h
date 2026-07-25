#pragma once

#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {

class MediaMasterClock {
public:
    virtual ~MediaMasterClock() = default;
    virtual ::media::Result<MediaRunningTime> now() const noexcept = 0;
};

} // namespace media::ffmpeg::graph
