#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncStartupPolicyPlanner final {
public:
    static ::media::Result<MediaAvSyncStartupPolicy> plan(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaAvSyncStartupPolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph
