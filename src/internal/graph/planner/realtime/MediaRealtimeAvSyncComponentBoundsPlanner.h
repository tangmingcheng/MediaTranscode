#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanCore;

class MediaRealtimeAvSyncComponentBoundsPlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncComponentBounds> plan(
        const MediaRealtimeRtpTranscodePlanCore& plan);

private:
    MediaRealtimeAvSyncComponentBoundsPlanner() = delete;
};

} // namespace media::ffmpeg::graph
