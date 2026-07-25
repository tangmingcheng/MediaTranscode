#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;

class MediaRealtimeAvSyncRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncRuntimePlan> plan(
        MediaRealtimeRtpTranscodePlan& outer,
        MediaAvSyncPlan synchronization);

};

} // namespace media::ffmpeg::graph
