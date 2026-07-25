#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalidSchedule(std::string message)
{
    return ::media::ErrorInfo::invalidArgument(std::move(message));
}

bool validDeadline(MediaRunningTime value) noexcept
{
    return value.nanoseconds() >= 0;
}

} // namespace

MediaRtcpSenderReportCommitToken::MediaRtcpSenderReportCommitToken(
    std::uint64_t generation,
    std::uint64_t revision,
    MediaRunningTime expectedDeadline,
    MediaRunningTime nextDeadline) noexcept
    : m_generation(generation),
      m_revision(revision),
      m_expectedDeadline(expectedDeadline),
      m_nextDeadline(nextDeadline)
{
}

MediaRtcpSenderReportScheduleDecision::MediaRtcpSenderReportScheduleDecision(
    MediaRunningTime scheduledDeadlineValue,
    MediaRunningTime reportInstantValue,
    MediaRunningTime nextDeadlineValue,
    MediaRunningTime latenessValue,
    std::uint64_t skippedIntervalsValue,
    MediaRtcpSenderReportCommitToken commitTokenValue)
    : scheduledDeadline(scheduledDeadlineValue),
      reportInstant(reportInstantValue),
      nextDeadline(nextDeadlineValue),
      lateness(latenessValue),
      skippedIntervals(skippedIntervalsValue),
      commitToken(std::move(commitTokenValue))
{
}

MediaRtcpSenderReportSchedule::MediaRtcpSenderReportSchedule(
    MediaRunningTime initialDeadline,
    MediaRunningTime interval,
    MediaRunningTime maximumLateness,
    std::uint64_t generation) noexcept
    : m_nextDeadline(initialDeadline),
      m_interval(interval),
      m_maximumLateness(maximumLateness),
      m_generation(generation),
      m_revision(1)
{
}

::media::Result<MediaRtcpSenderReportSchedule>
MediaRtcpSenderReportSchedule::create(
    MediaRunningTime initialDeadline,
    MediaRunningTime interval,
    MediaRunningTime maximumLateness,
    std::uint64_t generation) noexcept
{
    if (!validDeadline(initialDeadline) || interval.nanoseconds() <= 0 ||
        maximumLateness.nanoseconds() < 0 || generation == 0) {
        return ::media::Result<MediaRtcpSenderReportSchedule>::failure(
            invalidSchedule(
                "RTCP sender report schedule parameters are incomplete: "
                "initial_deadline_ns=" +
                std::to_string(initialDeadline.nanoseconds()) +
                " interval_ns=" + std::to_string(interval.nanoseconds()) +
                " maximum_lateness_ns=" +
                std::to_string(maximumLateness.nanoseconds()) +
                " generation=" + std::to_string(generation)));
    }
    return ::media::Result<MediaRtcpSenderReportSchedule>::success(
        MediaRtcpSenderReportSchedule(
            initialDeadline, interval, maximumLateness, generation));
}

::media::Result<std::optional<MediaRtcpSenderReportScheduleDecision>>
MediaRtcpSenderReportSchedule::prepare(
    MediaRunningTime now,
    std::uint64_t generation) const noexcept
{
    using PrepareResult = ::media::Result<
        std::optional<MediaRtcpSenderReportScheduleDecision>>;
    if (!validDeadline(now)) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report time must be non-negative"));
    }
    if (generation == 0 || generation != m_generation) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report generation does not match"));
    }
    if (m_revision == std::numeric_limits<std::uint64_t>::max()) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report schedule revision exhausted"));
    }
    if (now < m_nextDeadline) {
        return PrepareResult::success(std::nullopt);
    }
    auto latenessResult = now.checkedSubtract(m_nextDeadline);
    if (!latenessResult) {
        return PrepareResult::failure(latenessResult.error());
    }
    const MediaRunningTime lateness = latenessResult.value();
    if (lateness > m_maximumLateness) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report exceeded maximum lateness"));
    }

    const std::uint64_t intervalNs = static_cast<std::uint64_t>(
        m_interval.nanoseconds());
    const std::uint64_t latenessNs = static_cast<std::uint64_t>(
        lateness.nanoseconds());
    const std::uint64_t steps = latenessNs / intervalNs + 1;
    const std::int64_t deadlineNs = m_nextDeadline.nanoseconds();
    const std::uint64_t maximumSteps = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max() - deadlineNs) /
        intervalNs;
    if (steps > maximumSteps) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report deadline advancement overflow"));
    }
    const auto advanceNs = static_cast<std::int64_t>(steps * intervalNs);
    const MediaRunningTime advanced = MediaRunningTime::fromNanoseconds(
        deadlineNs + advanceNs);
    MediaRtcpSenderReportCommitToken token(
        m_generation, m_revision, m_nextDeadline, advanced);
    return PrepareResult::success(MediaRtcpSenderReportScheduleDecision(
        m_nextDeadline,
        now,
        advanced,
        lateness,
        steps - 1,
        std::move(token)));
}

::media::Status MediaRtcpSenderReportSchedule::commit(
    const MediaRtcpSenderReportCommitToken& token) noexcept
{
    if (token.m_generation != m_generation ||
        token.m_revision != m_revision ||
        token.m_expectedDeadline != m_nextDeadline ||
        token.m_nextDeadline <= m_nextDeadline ||
        m_revision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            invalidSchedule(
                "RTCP sender report commit token is stale or invalid"));
    }
    m_nextDeadline = token.m_nextDeadline;
    ++m_revision;
    return ::media::Status::success();
}

::media::Status MediaRtcpSenderReportSchedule::reset(
    MediaRunningTime initialDeadline,
    std::uint64_t generation) noexcept
{
    if (!validDeadline(initialDeadline) || generation <= m_generation ||
        m_revision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            invalidSchedule(
                "RTCP sender report reset requires a newer generation and explicit deadline"));
    }
    m_nextDeadline = initialDeadline;
    m_generation = generation;
    ++m_revision;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
