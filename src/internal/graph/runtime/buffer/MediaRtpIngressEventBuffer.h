#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

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

struct MediaRtpClockInvalidation final {
    std::uint64_t generation;
};

class MediaRtpIngressEventBuffer final : public MediaBuffer {
public:
    explicit MediaRtpIngressEventBuffer(MediaRtcpClockEvidence evidence);
    MediaRtpIngressEventBuffer(MediaRtpDiscontinuity discontinuity,
                               std::uint64_t generation);
    explicit MediaRtpIngressEventBuffer(MediaRtpClockObservation observation);
    explicit MediaRtpIngressEventBuffer(MediaRtpClockInvalidation invalidation);

    MediaBufferType type() const noexcept override;
    MediaRtpIngressEventKind kind() const noexcept;
    const std::optional<MediaRtcpClockEvidence>& clockEvidence() const noexcept;
    const std::optional<MediaRtpDiscontinuity>& discontinuity() const noexcept;
    const std::optional<std::uint64_t>& discontinuityGeneration() const noexcept;
    const std::optional<MediaRtpClockObservation>& clockObservation() const noexcept;
    const std::optional<MediaRtpClockInvalidation>& clockInvalidation() const noexcept;

private:
    MediaRtpIngressEventKind m_kind;
    std::optional<MediaRtcpClockEvidence> m_clockEvidence;
    std::optional<MediaRtpDiscontinuity> m_discontinuity;
    std::optional<std::uint64_t> m_discontinuityGeneration;
    std::optional<MediaRtpClockObservation> m_clockObservation;
    std::optional<MediaRtpClockInvalidation> m_clockInvalidation;
};

} // namespace media::ffmpeg::graph
