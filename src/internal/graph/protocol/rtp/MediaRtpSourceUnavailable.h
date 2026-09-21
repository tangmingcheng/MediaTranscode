#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaRtpSourceUnavailableReason : std::uint8_t {
    ClockEvidenceExpired,
    SenderLeft,
    TransportDiscontinuity
};

struct MediaRtpClockInvalidation final {
    std::uint64_t generation;
    MediaRtpSourceUnavailableReason reason;
    std::int64_t observedAtNs;
};

} // namespace media::ffmpeg::graph
