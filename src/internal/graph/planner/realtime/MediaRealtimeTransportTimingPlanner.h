#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeTransportTimingPlanner final {
public:
    static ::media::Result<MediaRealtimeTransportTimingPlan> plan(
        const MediaRealtimeDeploymentLatencyBudget& latency);

private:
    MediaRealtimeTransportTimingPlanner() = delete;
};

} // namespace media::ffmpeg::graph
