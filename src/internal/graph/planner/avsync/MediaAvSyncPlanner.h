#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncPlanner final {
public:
    static ::media::Result<MediaAvSyncPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaAvSyncPlanner() = delete;
};

} // namespace media::ffmpeg::graph
