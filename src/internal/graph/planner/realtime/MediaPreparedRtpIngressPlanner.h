#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"

namespace media::ffmpeg::graph {

class MediaPreparedRealtimeInput;
struct MediaRealtimeRtpTransportPlan;

class MediaPreparedRtpIngressPlanner final {
public:
    static ::media::Result<MediaRtpIngressPlan> plan(
        MediaPreparedRealtimeInput& input);
    static ::media::Status bind(
        const MediaRtpIngressPlan& ingress,
        MediaRealtimeRtpTransportPlan& transport);

private:
    MediaPreparedRtpIngressPlanner() = delete;
};

} // namespace media::ffmpeg::graph
