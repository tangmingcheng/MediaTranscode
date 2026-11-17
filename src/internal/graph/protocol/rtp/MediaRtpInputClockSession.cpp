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

::media::Result<std::uint64_t> MediaRtpInputClockSession::observeMedia(
    std::uint32_t ssrc)
{
    const auto generationBefore = m_tracker.generation();
    if (auto status = m_tracker.observeMedia(ssrc); !status)
        return ::media::Result<std::uint64_t>::failure(status.error());
    return ::media::Result<std::uint64_t>::success(generationBefore);
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::prepareDiscontinuity(
    std::uint64_t generationBefore) const
{
    using Result = ::media::Result<MediaRtpPreparedClockObservation>;
    auto prepared = m_tracker.prepareContinuityChange(m_tracker.generation() == generationBefore);
    if (!prepared) return Result::failure(prepared.error());
    auto schedule = m_schedule; schedule.reset();
    return Result::success({std::move(prepared).value(), std::move(schedule), false, std::nullopt, false});
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::prepareRtcp(
    std::span<const std::uint8_t> datagram, std::int64_t observedAtNs, std::int64_t evaluationAtNs) const try
{
    using Result = ::media::Result<MediaRtpPreparedClockObservation>;
    if (observedAtNs <= 0) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP clock session requires a monotonic RTCP arrival time"));
    auto packets = MediaRtcpCompoundParser::parse(datagram,
        MediaRtcpCompoundPolicy{m_config.compositionMode, m_config.tracker.requireCname});
    if (!packets) return Result::failure(packets.error());
    auto prepared = m_tracker.prepare(packets.value(), observedAtNs, evaluationAtNs);
    if (!prepared) return Result::failure(prepared.error());
    return materializeObservation(std::move(prepared).value());
}

catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtpPreparedClockObservation>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock observation allocation failed"));
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::prepareEvidence(std::int64_t now) const
{
    using Result = ::media::Result<MediaRtpPreparedClockObservation>;
    auto prepared = m_tracker.prepareEvidence(now);
    if (!prepared) return Result::failure(prepared.error());
    return materializeObservation(std::move(prepared).value());
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::materializeObservation(
    MediaRtcpPreparedObservation prepared) const
{
    using Result = ::media::Result<MediaRtpPreparedClockObservation>;
    auto schedule = m_schedule;
    if (prepared.invalidated) schedule.reset();
    if (const auto* evidence = std::get_if<MediaRtcpClockEvidence>(&prepared.output)) {
        auto status = schedule.observeEvidence(evidence->senderReportObservedAtNs, evidence->cnameObservedAtNs);
        if (!status) {
            prepared.rejection = status.error();
            prepared.output = std::monostate{};
        }
    }
    const bool evidencePublished = std::holds_alternative<MediaRtcpClockEvidence>(prepared.output);
    return Result::success({std::move(prepared), std::move(schedule), evidencePublished, std::nullopt, false});
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::prepareClockTransition(std::int64_t now) const
{
    using Result = ::media::Result<MediaRtpPreparedClockObservation>;
    auto schedule = m_schedule;
    auto age = schedule.transition(now);
    if (!age) return Result::failure(age.error());
    const bool invalidated = age.value() && m_config.lossPolicy == MediaRtpClockLossPolicy::InvalidateAndWait;
    auto prepared = m_tracker.prepareContinuityChange(invalidated);
    if (!prepared) return Result::failure(prepared.error());
    if (invalidated) {
        schedule.reset();
        prepared.value().output = MediaRtpClockInvalidation{prepared.value().state.generation,
            MediaRtpSourceUnavailableReason::ClockEvidenceExpired, now};
    }
    const bool retire = age.value() && (m_config.lossPolicy == MediaRtpClockLossPolicy::WaitForEvidence || invalidated);
    return Result::success({std::move(prepared).value(), std::move(schedule), false, age.value(), retire});
}

::media::Status MediaRtpInputClockSession::commitClockObservation(MediaRtpPreparedClockObservation&& prepared)
{
    if (auto status = m_tracker.commit(std::move(prepared.tracker)); !status) return status;
    m_schedule = std::move(prepared.schedule);
    return ::media::Status::success();
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputClockSession::prepareMemberRetirement(
    std::uint64_t expectedIngressGeneration, std::uint32_t expectedSsrc,
    MediaRtpSourceUnavailableReason reason, std::int64_t observedAtNs) const
{
    auto prepared = m_tracker.prepareMemberRetirement(expectedIngressGeneration, expectedSsrc, reason, observedAtNs);
    if (!prepared) return ::media::Result<MediaRtpPreparedClockObservation>::failure(prepared.error());
    return materializeObservation(std::move(prepared).value());
}

::media::Result<int> MediaRtpInputClockSession::receiveTimeoutMs(
    std::int64_t observedAtNs, int plannedMaximumMs) const noexcept
{
    return m_schedule.receiveTimeoutMs(observedAtNs, plannedMaximumMs);
}

std::optional<std::int64_t> MediaRtpInputClockSession::nextDeadlineNs() const noexcept
{ return m_schedule.nextDeadlineNs(); }

std::uint64_t MediaRtpInputClockSession::generation() const noexcept
{
    return m_tracker.generation();
}

const MediaRtpInputClockSessionConfig& MediaRtpInputClockSession::config() const noexcept
{
    return m_config;
}

} // namespace media::ffmpeg::graph
