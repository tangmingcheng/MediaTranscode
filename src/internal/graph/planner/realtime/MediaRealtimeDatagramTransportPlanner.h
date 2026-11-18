#pragma once

#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"

namespace media::ffmpeg::graph {

class MediaRealtimeDatagramTransportPlanner final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaVideoOnlySeparateRtpOutputRuntimePlan& output,
        const MediaPreparedRealtimeEmissionSet& emission);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaSeparateRtpOutputRuntimePlan& output,
        const MediaPreparedRealtimeEmissionSet& emission);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaProjectMpegTsRuntimeOutputPlan& output,
        const MediaPreparedRealtimeEmissionSet& emission);

private:
    MediaRealtimeDatagramTransportPlanner() = delete;
};

} // namespace media::ffmpeg::graph
