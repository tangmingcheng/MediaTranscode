#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanCore;
struct MediaRealtimeOutputPlanningDraft;

class MediaRealtimeAvSyncRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncRuntimePlan> plan(
        MediaRealtimeRtpTranscodePlanCore& outer,
        MediaRealtimeOutputPlanningDraft& output,
        MediaAvSyncPlan synchronization);

};

} // namespace media::ffmpeg::graph
