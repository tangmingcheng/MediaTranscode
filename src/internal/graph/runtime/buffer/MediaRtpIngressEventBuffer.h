#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <optional>

namespace media::ffmpeg::graph {

enum class MediaRtpIngressEventKind {
    ClockEvidence,
    Discontinuity
};

class MediaRtpIngressEventBuffer final : public MediaBuffer {
public:
    explicit MediaRtpIngressEventBuffer(MediaRtcpClockEvidence evidence);
    explicit MediaRtpIngressEventBuffer(MediaRtpDiscontinuity discontinuity);

    MediaBufferType type() const noexcept override;
    MediaRtpIngressEventKind kind() const noexcept;
    const std::optional<MediaRtcpClockEvidence>& clockEvidence() const noexcept;
    const std::optional<MediaRtpDiscontinuity>& discontinuity() const noexcept;

private:
    MediaRtpIngressEventKind m_kind;
    std::optional<MediaRtcpClockEvidence> m_clockEvidence;
    std::optional<MediaRtpDiscontinuity> m_discontinuity;
};

} // namespace media::ffmpeg::graph
