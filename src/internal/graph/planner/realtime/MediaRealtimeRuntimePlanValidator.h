#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlan;

class MediaRealtimeRuntimePlanValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeRtpTranscodePlan& plan);

private:
    MediaRealtimeRuntimePlanValidator() = delete;
};

} // namespace media::ffmpeg::graph
