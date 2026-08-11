#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

namespace media::ffmpeg::graph {

class MediaRealtimeRtpInputPlanValidator final {
public:
    static ::media::Status validate(
        RealtimeInputType inputType,
        const MediaRealtimeRtpInputNodePlan& input);

private:
    MediaRealtimeRtpInputPlanValidator() = delete;
};

} // namespace media::ffmpeg::graph
