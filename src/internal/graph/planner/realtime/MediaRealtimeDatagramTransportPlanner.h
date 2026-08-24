#pragma once

#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"

namespace media::ffmpeg::graph {

class MediaRealtimeDatagramTransportPlanner final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaVideoOnlySeparateRtpOutputRuntimePlan& output);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaSeparateRtpOutputRuntimePlan& output);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaProjectMpegTsRuntimeOutputPlan& output);

private:
    MediaRealtimeDatagramTransportPlanner() = delete;
};

} // namespace media::ffmpeg::graph
