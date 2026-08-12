#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;
struct MediaRealtimeVideoRuntimePlan;

class MediaRealtimeVideoRuntimePlanValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeRtpTranscodePlan& outer,
        const MediaRealtimeVideoRuntimePlan& runtime);

private:
    MediaRealtimeVideoRuntimePlanValidator() = delete;
};

} // namespace media::ffmpeg::graph
