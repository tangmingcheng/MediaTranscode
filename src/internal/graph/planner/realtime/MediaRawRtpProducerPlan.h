#pragma once
#include "internal/graph/planner/realtime/MediaDatagramProtocolTopology.h"
#include "internal/graph/protocol/rtp/MediaRtpAccessUnitEmissionContract.h"
#include <span>

namespace media::ffmpeg::graph {
struct MediaRawRtpProducerPlan final {
    MediaDatagramProtocolEndpointRole role;
    MediaRtpAccessUnitEmissionContract emission;
    std::uint64_t maximumRtcpCompoundBytes;
    friend bool operator==(const MediaRawRtpProducerPlan&,
                           const MediaRawRtpProducerPlan&) = default;
};
::media::Status validateRawRtpProducerPlans(MediaDatagramProtocolLayout layout,
    std::span<const MediaRawRtpProducerPlan> producers);
} // namespace media::ffmpeg::graph
