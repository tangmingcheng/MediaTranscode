#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRawRtpPreparedStreamPlan final {
    MediaRtpUdpTransportConfig transport;
    MediaPreparedRawRtpIdentity identity;
};

struct MediaRawRtpProbePlan final {
    MediaRawRtpPreparedStreamPlan video;
    std::optional<MediaRawRtpPreparedStreamPlan> audio;
    int openTimeoutMs = 0;
    int analyzeDurationUs = 0;
    std::size_t maximumBufferedBytes = 0;
    std::size_t reorderWindowPackets = 0;
    int maximumReorderDelayMs = 0;
    std::optional<MediaRtpVideoPacketizationPolicy> packetizationPolicy;
    std::optional<MediaRtcpCompoundPolicy> rtcpPolicy;
};

struct MediaPreparedRawRtpProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaPreparedRealtimeInput video;
    std::optional<MediaPreparedRealtimeInput> audio;
};

class MediaRawRtpInputPreparer final {
public:
    static ::media::Result<MediaPreparedRawRtpProbe> prepare(
        const MediaRawRtpProbePlan& plan);

private:
    MediaRawRtpInputPreparer() = delete;
};

} // namespace media::ffmpeg::graph
