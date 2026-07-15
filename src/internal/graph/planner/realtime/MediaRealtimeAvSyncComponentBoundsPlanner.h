#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;

class MediaRealtimeAvSyncComponentBoundsPlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncComponentBounds> plan(
        const MediaRealtimeRtpTranscodePlan& plan);

private:
    MediaRealtimeAvSyncComponentBoundsPlanner() = delete;
};

} // namespace media::ffmpeg::graph
