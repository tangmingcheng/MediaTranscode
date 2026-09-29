#pragma once

#include "internal/graph/planner/realtime/MediaPreparedInputRetentionPlan.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRawInputPlan;
class MediaPreparedRealtimeInput;

class MediaPreparedRtpStartupPlanner final {
public:
    static ::media::Result<MediaPreparedInputRetentionPlan> plan(
        const MediaRealtimeRawInputPlan& input,
        const MediaPreparedRealtimeInput& video,
        const MediaPreparedRealtimeInput& audio,
        MediaRunningTime acquisitionWindow);
};

} // namespace media::ffmpeg::graph
