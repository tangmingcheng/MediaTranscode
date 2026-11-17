#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaRtpSourceUnavailableReason : std::uint8_t {
    ClockEvidenceExpired,
    SenderLeft,
    TransportDiscontinuity,
    GroupClosed
};

struct MediaRtpClockInvalidation final {
    std::uint64_t generation;
    MediaRtpSourceUnavailableReason reason;
    std::int64_t observedAtNs;
};

// The validated BYE identifies the departing member before invalidation.
// This is an ingress identity, not the next ingress or playback generation.
struct MediaRtpSenderDeparture final {
    std::uint64_t ingressGeneration;
    std::uint32_t ssrc;
    std::int64_t observedAtNs;
};

} // namespace media::ffmpeg::graph
