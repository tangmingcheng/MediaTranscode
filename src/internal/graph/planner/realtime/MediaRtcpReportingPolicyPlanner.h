#pragma once

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/protocol/rtp/MediaRtcpReportingPolicy.h"

namespace media::ffmpeg::graph {

class MediaRtcpReportingPolicyPlanner final {
public:
    static ::media::Result<MediaRtcpReportingPolicy> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaNumericIpAddress& remoteAddress,
        std::uint64_t sessionBandwidthBytesPerSecond,
        std::string bandwidthAuthority,
        std::uint64_t compoundPacketBytes);

private:
    MediaRtcpReportingPolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph
