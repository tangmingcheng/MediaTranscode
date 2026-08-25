#pragma once

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpRemoteEndpointPair.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaScheduledRtpOutputPlan final {
    MediaScheduledStream stream;
    MediaRtpRemoteEndpointPair transport;
    MediaScheduledRtpPacketizationPlan packetization;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    int clockRate;
    std::string cname;
    MediaRunningTime senderLead;
    MediaRunningTime senderReportInterval;
    friend bool operator==(const MediaScheduledRtpOutputPlan&,
                           const MediaScheduledRtpOutputPlan&) = default;
};

} // namespace media::ffmpeg::graph
