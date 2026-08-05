#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRawRtpProbePlan final {
    MediaRtpUdpTransportConfig transport;
    std::string codecName;
    std::uint8_t payloadType = 0;
    int clockRate = 0;
    int openTimeoutMs = 0;
    int analyzeDurationUs = 0;
    std::size_t maximumBufferedBytes = 0;
};

struct MediaPreparedRawRtpProbe final {
    MediaDetectedRtpVideoSignaling signaling;
    MediaPreparedRealtimeInput prepared;
};

class MediaRawRtpInputPreparer final {
public:
    static ::media::Result<MediaPreparedRawRtpProbe> prepare(
        const MediaRawRtpProbePlan& plan);

private:
    MediaRawRtpInputPreparer() = delete;
};

} // namespace media::ffmpeg::graph
