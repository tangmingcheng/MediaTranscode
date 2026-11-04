#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtpSourceUnavailable.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaRtpIngressEventKind {
    ClockEvidence,
    Discontinuity,
    ClockObservation,
    ClockInvalidation
};

struct MediaRtpClockObservation final {
    std::int64_t observedAtNs;
};

// Borrowed protocol values; the owner must outlive a synchronous consume call.
struct MediaRtpIngressEventView final {
    MediaRtpIngressEventKind kind;
    std::uint64_t sequence;
    const std::optional<MediaRtcpClockEvidence>& clockEvidence;
    const std::optional<MediaRtpDiscontinuity>& discontinuity;
    const std::optional<std::uint64_t>& discontinuityGeneration;
    const std::optional<MediaRtpClockObservation>& clockObservation;
    const std::optional<MediaRtpClockInvalidation>& clockInvalidation;
};

} // namespace media::ffmpeg::graph
