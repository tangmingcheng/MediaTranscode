#pragma once

#include "internal/graph/planner/realtime/MediaScheduledDatagramPacingPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaScheduledDatagramPacingPlanner final {
public:
    static ::media::Result<MediaScheduledDatagramPacingPlan> plan(
        const MediaRtpUdpSenderConfig& transport);

private:
    MediaScheduledDatagramPacingPlanner() = delete;
};

} // namespace media::ffmpeg::graph
