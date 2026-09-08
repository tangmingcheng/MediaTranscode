#pragma once

#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

namespace media::ffmpeg::graph {

class MediaRealtimeDatagramTransportPlanner final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaVideoOnlySeparateRtpOutputRuntimePlan& output,
        const MediaPipelinePlan& videoPipeline,
        MediaRational outputFrameRate);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaSeparateRtpOutputRuntimePlan& output,
        const MediaPipelinePlan& videoPipeline,
        MediaRational outputFrameRate,
        const MediaAudioPipelinePlan& audioPipeline);
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const std::string& sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaProjectMpegTsRuntimeOutputPlan& output,
        const MediaPipelinePlan& videoPipeline,
        MediaRational outputFrameRate,
        const MediaAudioPipelinePlan* audioPipeline);

private:
    MediaRealtimeDatagramTransportPlanner() = delete;
};

} // namespace media::ffmpeg::graph
