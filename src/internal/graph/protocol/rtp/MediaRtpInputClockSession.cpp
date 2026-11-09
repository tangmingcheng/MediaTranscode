#include "internal/graph/protocol/rtp/MediaRtpInputClockSession.h"

#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpInputClockSession> MediaRtpInputClockSession::create(
    MediaRtpInputClockSessionConfig config, MediaLineageAllocationScope allocation) try
{
    using Result = ::media::Result<MediaRtpInputClockSession>;
    if (const auto* prepared = std::get_if<MediaPreparedLineageAllocation>(&allocation);
        prepared && !prepared->resource)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP clock requires an owning allocation resource"));
    if (config.compositionMode != MediaRtcpCompositionMode::StrictCompoundRfc3550 &&
        config.compositionMode != MediaRtcpCompositionMode::ReducedSizeRfc5506) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP clock session requires a planned RTCP composition mode"));
    }
    auto schedule = MediaRtpClockObservationSchedule::create(
        config.tracker.senderReportTimeoutNs, config.maximumExtrapolationNs,
        config.tracker.cnameTimeoutNs, config.lossPolicy);
    if (!schedule) return Result::failure(schedule.error());
    return Result::success(MediaRtpInputClockSession(config, std::move(schedule).value(), std::move(allocation)));
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtpInputClockSession>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock session allocation failed"));
}


MediaRtpInputClockSession::MediaRtpInputClockSession(
    MediaRtpInputClockSessionConfig config, MediaRtpClockObservationSchedule schedule,
    MediaLineageAllocationScope allocation)
    : m_allocation(std::move(allocation)), m_config(config),
      m_tracker(config.tracker, m_allocation), m_schedule(std::move(schedule))
{
}

std::uint64_t MediaRtpInputClockSession::observeMedia(
    std::uint32_t ssrc)
{
    const auto generationBefore = m_tracker.generation();
    m_tracker.observeMedia(ssrc);
    return generationBefore;
}

void MediaRtpInputClockSession::observeDiscontinuity(
    std::uint64_t generationBefore) noexcept
{
    if (m_tracker.generation() == generationBefore) m_tracker.observeContinuityLoss();
    m_schedule.reset();
}

::media::Result<MediaRtcpObservation> MediaRtpInputClockSession::observeRtcp(
    std::span<const std::uint8_t> datagram, std::int64_t observedAtNs) try
{
    using Result = ::media::Result<MediaRtcpObservation>;
    if (observedAtNs <= 0) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP clock session requires a monotonic RTCP arrival time"));
    auto packets = MediaRtcpCompoundParser::parse(datagram,
        MediaRtcpCompoundPolicy{m_config.compositionMode, m_config.tracker.requireCname});
    if (!packets) return Result::failure(packets.error());
    auto observation = m_tracker.observe(packets.value(), observedAtNs);
    if (!observation) return Result::failure(observation.error());
    if (std::holds_alternative<MediaRtpClockInvalidation>(observation.value()))
        m_schedule.reset();
    return observation;
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtcpObservation>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock observation allocation failed"));
}


::media::Result<std::optional<MediaRtcpClockEvidence>>
MediaRtpInputClockSession::takeEvidenceUpdate(std::int64_t observedAtNs) try
{
    auto update = m_tracker.takeEvidenceUpdate(observedAtNs);
    if (!update || !update.value()) return update;
    const auto& evidence = *update.value();
    if (auto status = m_schedule.observeEvidence(evidence.senderReportObservedAtNs,
            evidence.cnameObservedAtNs); !status) {
        return ::media::Result<std::optional<MediaRtcpClockEvidence>>::failure(status.error());
    }
    return update;
}
catch (const std::bad_alloc&) {
    return ::media::Result<std::optional<MediaRtcpClockEvidence>>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock evidence allocation failed"));
}


::media::Result<int> MediaRtpInputClockSession::receiveTimeoutMs(
    std::int64_t observedAtNs, int plannedMaximumMs) const noexcept
{
    return m_schedule.receiveTimeoutMs(observedAtNs, plannedMaximumMs);
}

::media::Result<std::optional<MediaRtpClockSessionTransition>>
MediaRtpInputClockSession::transition(std::int64_t observedAtNs)
{
    using Result = ::media::Result<std::optional<MediaRtpClockSessionTransition>>;
    auto age = m_schedule.transition(observedAtNs);
    if (!age) return Result::failure(age.error());
    if (!age.value()) return Result::success(std::nullopt);
    MediaRtpClockSessionTransition result{*age.value(), std::nullopt};
    if (m_config.lossPolicy == MediaRtpClockLossPolicy::InvalidateAndWait) {
        m_tracker.observeContinuityLoss();
        m_schedule.reset();
        result.invalidation = MediaRtpClockInvalidation{m_tracker.generation(),
            MediaRtpSourceUnavailableReason::ClockEvidenceExpired, observedAtNs};
    }
    return Result::success(std::move(result));
}

std::uint64_t MediaRtpInputClockSession::generation() const noexcept
{
    return m_tracker.generation();
}

const MediaRtpInputClockSessionConfig& MediaRtpInputClockSession::config() const noexcept
{
    return m_config;
}

} // namespace media::ffmpeg::graph
