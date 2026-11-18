#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpBytes.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtcpNtpTimestamp final {
    uint32_t seconds;
    uint32_t fraction;

    friend bool operator==(const MediaRtcpNtpTimestamp&, const MediaRtcpNtpTimestamp&) = default;
};

struct MediaRtcpClockEvidence final {
    uint32_t observedMediaSsrc;
    uint32_t senderReportSsrc;
    uint32_t cnameSsrc;
    MediaRtcpNtpTimestamp ntp;
    uint32_t rtpTimestamp;
    MediaRtcpBytes cname{MediaLineageAllocator<std::uint8_t>{MediaRuntimeLineageAllocation{}}};
    int64_t senderReportObservedAtNs;
    int64_t cnameObservedAtNs;
    uint64_t generation;
};

} // namespace media::ffmpeg::graph
