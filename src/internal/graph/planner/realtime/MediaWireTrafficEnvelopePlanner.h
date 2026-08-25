#pragma once

#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaWireTrafficEnvelope.h"

namespace media::ffmpeg::graph {

class MediaWireTrafficEnvelopePlanner final {
public:
    static ::media::Result<MediaWireTrafficEnvelope> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaPreparedRealtimeEmissionSet& emission,
        const MediaVideoOnlySeparateRtpOutputRuntimePlan& output);
    static ::media::Result<MediaWireTrafficEnvelope> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaPreparedRealtimeEmissionSet& emission,
        const MediaSeparateRtpOutputRuntimePlan& output);
    static ::media::Result<MediaWireTrafficEnvelope> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaPreparedRealtimeEmissionSet& emission,
        const MediaProjectMpegTsRuntimeOutputPlan& output);

private:
    MediaWireTrafficEnvelopePlanner() = delete;
};

} // namespace media::ffmpeg::graph
