#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaAudioCorrectionReachabilityResult final {
    MediaAudioCorrectionReachabilityPlan correction;
    MediaRunningTime commandLead;
    MediaRunningTime compensationWindow;
    MediaRunningTime frequencyFilterTimeConstant;
};

class MediaAudioCorrectionReachabilityPlanner final {
public:
    static ::media::Result<MediaAudioCorrectionReachabilityResult> plan(
        const MediaAvSyncPlan& synchronization,
        const MediaRealtimeAvSyncPlanningFacts& facts);

private:
    MediaAudioCorrectionReachabilityPlanner() = delete;
};

} // namespace media::ffmpeg::graph
