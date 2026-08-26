#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanningDraft;
struct MediaRealtimeRtpTranscodeRequest;
struct MediaRealtimeOutputPlanningDraft;

class MediaRealtimeAvSyncRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncRuntimePlan> plan(
        MediaRealtimeRtpTranscodePlanningDraft& outer,
        MediaRealtimeOutputPlanningDraft& output,
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaAvSyncPlan synchronization,
        MediaRational outputFrameRate,
        const MediaPreparedRealtimeEmissionSet& preparedEmission);

};

} // namespace media::ffmpeg::graph
