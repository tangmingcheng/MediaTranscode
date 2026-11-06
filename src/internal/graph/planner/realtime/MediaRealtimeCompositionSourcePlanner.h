#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"

namespace media::ffmpeg::graph {

struct MediaCompositionSourceFixedProducts;

class MediaRealtimeCompositionSourcePlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionSourcePlan> plan(
        MediaCompositionSourceFixedProducts&& fixed,
        const MediaPreparedRtpSourceView& prepared,
        const MediaPreparedRealtimeEmissionSet& emission);
};

} // namespace media::ffmpeg::graph
