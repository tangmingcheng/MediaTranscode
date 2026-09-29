#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaThreadingPolicy;

struct MediaRealtimeRtpTranscodePlan;
struct MediaRealtimeAvSyncRuntimePlan;

class MediaRealtimeAvSyncRuntimePlanValidator final {
public:
    static ::media::Status validateThreading(const MediaThreadingPolicy& policy);
    static ::media::Status validate(
        const MediaRealtimeRtpTranscodePlan& outer,
        const MediaRealtimeAvSyncRuntimePlan& runtime);
};

} // namespace media::ffmpeg::graph
