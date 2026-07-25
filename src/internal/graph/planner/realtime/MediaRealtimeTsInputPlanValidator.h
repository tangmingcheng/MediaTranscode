#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

namespace media::ffmpeg::graph {

class MediaRealtimeTsInputPlanValidator final {
public:
    static ::media::Status validate(RealtimeInputType inputType,
                                    const MediaRealtimeRtpInputNodePlan& input);
};

} // namespace media::ffmpeg::graph
