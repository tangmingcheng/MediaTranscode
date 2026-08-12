#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

namespace media::ffmpeg::graph {

class MediaPreparedGenericInputPlanner final {
public:
    static ::media::Result<MediaPreparedGenericInputPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaAvSyncStartupPolicy& startup,
        int videoStreamIndex,
        MediaRational videoTimeBase,
        int audioStreamIndex,
        MediaRational audioTimeBase);

private:
    MediaPreparedGenericInputPlanner() = delete;
};

} // namespace media::ffmpeg::graph
