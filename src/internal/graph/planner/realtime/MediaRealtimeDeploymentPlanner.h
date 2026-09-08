#pragma once

#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaWireTrafficEnvelope.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeDeploymentBasePlan final {
    MediaRealtimeDeploymentServiceScope serviceScope;
    MediaRealtimeDeploymentMtuFact mtu;
    MediaRealtimeDeploymentLocalPortRange localPorts;
    MediaRealtimeDeploymentLatencyBudget latency;
    MediaRealtimeDeploymentObservationBudget observation;
    MediaRealtimeTransportTimingPlan transportTiming;
    std::optional<MediaRealtimeRtcpSessionCapability> rtcpSession;
    MediaWireTrafficEnvelope admittedWire;
    std::uint64_t provisionedWireCapacityBytesPerSecond = 0;
    std::uint64_t endpointCount = 0;
};

class MediaRealtimeDeploymentPlanner final {
public:
    static ::media::Result<MediaRealtimeDeploymentBasePlan> planBase(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaPreparedRealtimeEmissionSet& emission);

    static ::media::Result<MediaRealtimeDeploymentEnvelope> complete(
        MediaRealtimeDeploymentBasePlan base);

private:
    MediaRealtimeDeploymentPlanner() = delete;
};

} // namespace media::ffmpeg::graph
