#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaAudioPipelinePlan;
struct MediaGraphQueueParameters;

class MediaRealtimeAvSyncComponentBoundsPlanner final {
public:
    static ::media::Result<MediaRealtimeAvSyncComponentBounds> plan(
        const MediaGraphQueueParameters& queues,
        const MediaAudioPipelinePlan& audio);

private:
    MediaRealtimeAvSyncComponentBoundsPlanner() = delete;
};

} // namespace media::ffmpeg::graph
