#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanCore;
struct MediaRealtimeOutputPlanningDraft;
struct MediaAvSyncPlan;

class MediaRealtimeAvSyncPlanningFactsResolver final {
public:
    static ::media::Result<MediaRealtimeAvSyncPlanningFacts> resolve(
        const MediaRealtimeRtpTranscodePlanCore& plan,
        const MediaRealtimeOutputPlanningDraft& output,
        const MediaAvSyncPlan& synchronization);

private:
    MediaRealtimeAvSyncPlanningFactsResolver() = delete;
};

} // namespace media::ffmpeg::graph
