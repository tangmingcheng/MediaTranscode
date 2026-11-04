#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/planner/avsync/MediaAvSourceLifecyclePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

::media::Status validateMediaInitialSourceClockAvailability(
    MediaAvSourceLifecycleMode mode, bool outputActivated,
    MediaSourceClockReadiness readiness, bool discontinuity);

::media::Status validateMediaInitialClockGeneration(
    std::uint64_t generation, std::uint64_t plannedInitialGeneration);

} // namespace media::ffmpeg::graph
