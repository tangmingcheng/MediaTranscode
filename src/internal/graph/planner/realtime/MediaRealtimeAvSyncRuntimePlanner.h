#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;
struct MediaRealtimeOutputPlanningDraft;

class MediaRealtimeAvSyncRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncRuntimePlan> plan(
        MediaRealtimeRtpTranscodePlan& outer,
        MediaRealtimeOutputPlanningDraft& output,
        MediaAvSyncPlan synchronization);

};

} // namespace media::ffmpeg::graph
