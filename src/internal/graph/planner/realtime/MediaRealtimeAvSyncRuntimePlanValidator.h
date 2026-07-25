#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;

class MediaRealtimeAvSyncRuntimePlanValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeRtpTranscodePlan& outer);
};

} // namespace media::ffmpeg::graph
