#include "internal/graph/planner/realtime/MediaRtpInputWaitPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

namespace media::ffmpeg::graph {

::media::Status MediaRtpInputWaitPlan::validateInput(const MediaRealtimeInputConfig& input)
{
    if (input.inputLatencyMs && (input.type != RealtimeInputType::RtpPort ||
        *input.inputLatencyMs <= 0)) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Input latency requires RTP input and a positive value"));
    return ::media::Status::success();
}

::media::Result<MediaRtpInputWaitPlan> MediaRtpInputWaitPlan::plan(
    std::optional<int> inputLatencyMs, int analyzeDurationUs)
{
    using Result = ::media::Result<MediaRtpInputWaitPlan>;
    if (analyzeDurationUs <= 0 || (inputLatencyMs && *inputLatencyMs <= 0))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP input waiting requires positive analysis duration and declared latency"));
    if (inputLatencyMs) return Result::success(MediaRtpInputWaitPlan(
        MediaRtpInputWaitMode::DeclaredInputLatency,
        static_cast<std::int64_t>(*inputLatencyMs) * 1'000'000));
    return Result::success(MediaRtpInputWaitPlan(MediaRtpInputWaitMode::FiniteProbeObservation,
        static_cast<std::int64_t>(analyzeDurationUs) * 1'000));
}

MediaRtpInputWaitPlan::MediaRtpInputWaitPlan(
    MediaRtpInputWaitMode mode, std::int64_t nanoseconds) noexcept
    : m_mode(mode), m_nanoseconds(nanoseconds) {}

MediaRtpInputWaitMode MediaRtpInputWaitPlan::mode() const noexcept { return m_mode; }
std::int64_t MediaRtpInputWaitPlan::bootstrapNanoseconds() const noexcept { return m_nanoseconds; }
int MediaRtpInputWaitPlan::bootstrapMilliseconds() const noexcept
{ return static_cast<int>(m_nanoseconds / 1'000'000 + (m_nanoseconds % 1'000'000 != 0)); }

::media::Result<std::int64_t> MediaRtpInputWaitPlan::finalNanoseconds(
    std::int64_t observedMaximumInterarrivalNs) const
{
    const auto delay = m_mode == MediaRtpInputWaitMode::DeclaredInputLatency
        ? m_nanoseconds : observedMaximumInterarrivalNs;
    if (auto status = validateFinalDelay(delay); !status)
        return ::media::Result<std::int64_t>::failure(status.error());
    return ::media::Result<std::int64_t>::success(delay);
}

::media::Status MediaRtpInputWaitPlan::validateFinalDelay(std::int64_t nanoseconds) const
{
    if (nanoseconds <= 0 || (m_mode == MediaRtpInputWaitMode::DeclaredInputLatency &&
        nanoseconds != m_nanoseconds)) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP ingress waiting contradicts its selected input contract"));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
