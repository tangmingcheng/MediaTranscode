#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputConfig;

enum class MediaRtpInputWaitMode {
    FiniteProbeObservation,
    DeclaredInputLatency
};

// Selected once from caller facts before opening a socket. A declared budget
// cannot transition to the finite-probe mode after planning or capture failure.
class MediaRtpInputWaitPlan final {
public:
    static ::media::Status validateInput(const MediaRealtimeInputConfig& input);
    static ::media::Result<MediaRtpInputWaitPlan> plan(
        std::optional<int> inputLatencyMs, int analyzeDurationUs);
    MediaRtpInputWaitMode mode() const noexcept;
    std::int64_t bootstrapNanoseconds() const noexcept;
    int bootstrapMilliseconds() const noexcept;
    ::media::Result<std::int64_t> finalNanoseconds(
        std::int64_t observedMaximumInterarrivalNs) const;
    ::media::Status validateFinalDelay(std::int64_t nanoseconds) const;

private:
    MediaRtpInputWaitPlan(MediaRtpInputWaitMode mode, std::int64_t nanoseconds) noexcept;
    MediaRtpInputWaitMode m_mode;
    std::int64_t m_nanoseconds;
};

} // namespace media::ffmpeg::graph
