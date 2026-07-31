#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncRuntimePlan;
struct MediaRealtimeRtpTranscodePlan;

class MediaRealtimeAvSyncRuntimeOutputValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeRtpTranscodePlan& outer,
        const MediaRealtimeAvSyncRuntimePlan& runtime);

private:
    MediaRealtimeAvSyncRuntimeOutputValidator() = delete;
};

} // namespace media::ffmpeg::graph
