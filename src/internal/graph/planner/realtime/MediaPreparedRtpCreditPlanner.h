#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpCreditPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
struct MediaPreparedRtpSourceSessionStreamPlan;
class MediaPreparedRtpCreditPlanner final {
public:
    static ::media::Result<MediaPreparedRtpCreditPlan> plan(
        const MediaPreparedRtpSourceSessionStreamPlan& stream,
        std::size_t heldPayloadBytes, std::size_t packetObjects,
        std::size_t controlObjects, std::size_t maximumCnameBytes);
};
}
