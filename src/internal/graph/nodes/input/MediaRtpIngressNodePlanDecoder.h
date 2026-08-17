#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

namespace media::ffmpeg::graph {

class MediaRtpIngressNodePlanDecoder final {
public:
    static ::media::Result<MediaRtpIngressPlan> decode(
        const MediaNodeOptions* options);

private:
    MediaRtpIngressNodePlanDecoder() = delete;
};

} // namespace media::ffmpeg::graph
