#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

std::int64_t calculateDeadline(std::int64_t observedAtNs, std::int64_t intervalNs) noexcept
{
    if (observedAtNs > std::numeric_limits<std::int64_t>::max() - intervalNs) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return observedAtNs + intervalNs;
}

} // namespace

::media::Result<MediaRtpClockObservationSchedule> MediaRtpClockObservationSchedule::create(
    std::int64_t senderReportTimeoutNs,
    std::int64_t maximumExtrapolationNs,
    std::int64_t cnameTimeoutNs)
{
    if (senderReportTimeoutNs <= 0 || maximumExtrapolationNs <= senderReportTimeoutNs ||
        cnameTimeoutNs <= 0) {
        return ::media::Result<MediaRtpClockObservationSchedule>::failure(
            ::media::ErrorInfo::invalidArgument("RTP clock observation deadlines must be positive and ordered"));
    }
    return ::media::Result<MediaRtpClockObservationSchedule>::success(
        MediaRtpClockObservationSchedule(senderReportTimeoutNs,
                                         maximumExtrapolationNs,
                                         cnameTimeoutNs));
}

MediaRtpClockObservationSchedule::MediaRtpClockObservationSchedule(
    std::int64_t senderReportTimeoutNs,
    std::int64_t maximumExtrapolationNs,
    std::int64_t cnameTimeoutNs) noexcept
    : m_senderReportTimeoutNs(senderReportTimeoutNs)
    , m_maximumExtrapolationNs(maximumExtrapolationNs)
    , m_cnameTimeoutNs(cnameTimeoutNs)
{
}

::media::Status MediaRtpClockObservationSchedule::observeEvidence(
    std::int64_t senderReportObservedAtNs,
    std::int64_t cnameObservedAtNs) noexcept
{
    if (senderReportObservedAtNs < 0 || cnameObservedAtNs < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP clock evidence observation times must be non-negative"));
    }
    m_senderReportObservedAtNs = senderReportObservedAtNs;
    m_cnameObservedAtNs = cnameObservedAtNs;
    m_degradedPublished = false;
    m_expiredPublished = false;
    return ::media::Status::success();
}

std::optional<std::int64_t> MediaRtpClockObservationSchedule::nextDeadlineNs() const noexcept
{
    if (!m_senderReportObservedAtNs || !m_cnameObservedAtNs || m_expiredPublished) {
        return std::nullopt;
    }
    const std::int64_t srDeadline = calculateDeadline(
        *m_senderReportObservedAtNs,
        m_degradedPublished ? m_maximumExtrapolationNs : m_senderReportTimeoutNs);
    const std::int64_t cnameDeadline = calculateDeadline(*m_cnameObservedAtNs, m_cnameTimeoutNs);
    return std::min(srDeadline, cnameDeadline);
}

::media::Result<int> MediaRtpClockObservationSchedule::receiveTimeoutMs(
    std::int64_t observedAtNs,
    int plannedMaximumMs) const noexcept
{
    if (observedAtNs < 0 || plannedMaximumMs <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP clock receive timeout requires non-negative observation time and positive maximum"));
    }
    const auto deadline = nextDeadlineNs();
    if (!deadline || *deadline <= observedAtNs) {
        return ::media::Result<int>::success(deadline ? 1 : plannedMaximumMs);
    }
    const std::int64_t remainingNs = *deadline - observedAtNs;
    const std::int64_t ceilingMs = 1 + (remainingNs - 1) / 1'000'000;
    return ::media::Result<int>::success(
        static_cast<int>(std::min<std::int64_t>(plannedMaximumMs, ceilingMs)));
}

::media::Result<std::optional<MediaRtpClockAgeTransition>>
MediaRtpClockObservationSchedule::transition(
    std::int64_t observedAtNs) noexcept
{
    if (observedAtNs < 0) {
        return ::media::Result<std::optional<MediaRtpClockAgeTransition>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP clock transition observation time must be non-negative"));
    }
    const auto deadline = nextDeadlineNs();
    if (!deadline || observedAtNs <= *deadline) {
        return ::media::Result<std::optional<MediaRtpClockAgeTransition>>::success(std::nullopt);
    }
    const std::int64_t cnameDeadline = calculateDeadline(*m_cnameObservedAtNs, m_cnameTimeoutNs);
    if (observedAtNs >= cnameDeadline) {
        m_expiredPublished = true;
        return ::media::Result<std::optional<MediaRtpClockAgeTransition>>::success(
            MediaRtpClockAgeTransition::Expired);
    }
    if (!m_degradedPublished) {
        m_degradedPublished = true;
        return ::media::Result<std::optional<MediaRtpClockAgeTransition>>::success(
            MediaRtpClockAgeTransition::Degraded);
    }
    m_expiredPublished = true;
    return ::media::Result<std::optional<MediaRtpClockAgeTransition>>::success(
        MediaRtpClockAgeTransition::Expired);
}

void MediaRtpClockObservationSchedule::reset() noexcept
{
    m_senderReportObservedAtNs.reset();
    m_cnameObservedAtNs.reset();
    m_degradedPublished = false;
    m_expiredPublished = false;
}

} // namespace media::ffmpeg::graph
