#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRawRtpPreparedStreamPlan final {
    MediaRtpUdpTransportConfig transport;
    MediaPreparedRawRtpIdentity identity;
};

struct MediaRawRtpProbePlan final {
    struct VideoOnly final {
        MediaRawRtpPreparedStreamPlan video;
    };
    struct AudioVideo final {
        MediaRawRtpPreparedStreamPlan video;
        MediaRawRtpPreparedStreamPlan audio;
    };
    using Streams = std::variant<VideoOnly, AudioVideo>;

    Streams streams;
    int openTimeoutMs = 0;
    int analyzeDurationUs = 0;
    std::size_t maximumBufferedBytes = 0;
    std::size_t reorderWindowPackets = 0;
    int maximumReorderDelayMs = 0;
    std::optional<MediaRtpVideoPacketizationPolicy> packetizationPolicy;
    std::optional<MediaRtcpCompoundPolicy> rtcpPolicy;
};

struct MediaPreparedRawRtpVideoOnlyProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaPreparedRealtimeInput video;
};

struct MediaPreparedRawRtpAudioVideoProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaPreparedRealtimeInput video;
    MediaPreparedRealtimeInput audio;
};

using MediaPreparedRawRtpProbe = std::variant<
    MediaPreparedRawRtpVideoOnlyProbe,
    MediaPreparedRawRtpAudioVideoProbe>;

class MediaRawRtpInputPreparer final {
public:
    static ::media::Result<MediaPreparedRawRtpProbe> prepare(
        const MediaRawRtpProbePlan& plan);

private:
    MediaRawRtpInputPreparer() = delete;
};

} // namespace media::ffmpeg::graph
