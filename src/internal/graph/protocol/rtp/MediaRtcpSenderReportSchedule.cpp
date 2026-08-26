#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t RandomFactorMinimum = 500000;
constexpr std::uint64_t RandomFactorRange = 1000001;
constexpr std::uint64_t CompensationDivisor = 1218280;

::media::ErrorInfo invalidSchedule(std::string message)
{
    return ::media::ErrorInfo::invalidArgument(std::move(message));
}

bool validInstant(MediaRunningTime value) noexcept
{
    return value.nanoseconds() >= 0;
}

std::uint64_t nextRandomState(std::uint64_t state) noexcept
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

::media::Result<MediaRunningTime> randomizedInterval(
    MediaRunningTime base,
    std::uint64_t randomState) noexcept
{
    if (base.nanoseconds() <= 0 || randomState == 0) {
        return ::media::Result<MediaRunningTime>::failure(
            invalidSchedule("RTCP random interval requires a positive base and non-zero state"));
    }
    const auto baseNs = static_cast<std::uint64_t>(base.nanoseconds());
    const auto factor = RandomFactorMinimum +
        randomState % RandomFactorRange;
    const auto quotient = baseNs / CompensationDivisor;
    const auto remainder = baseNs % CompensationDivisor;
    if (quotient > static_cast<std::uint64_t>(
                       std::numeric_limits<std::int64_t>::max()) /
                       factor) {
        return ::media::Result<MediaRunningTime>::failure(
            invalidSchedule("RTCP randomized interval overflow"));
    }
    const auto whole = quotient * factor;
    const auto fractional = remainder * factor / CompensationDivisor;
    if (whole > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) - fractional) {
        return ::media::Result<MediaRunningTime>::failure(
            invalidSchedule("RTCP randomized interval overflow"));
    }
    const auto intervalNs = whole + fractional;
    if (intervalNs == 0) {
        return ::media::Result<MediaRunningTime>::failure(
            invalidSchedule("RTCP randomized interval rounded to zero"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(intervalNs)));
}

} // namespace

MediaRtcpSenderReportCommitToken::MediaRtcpSenderReportCommitToken(
    std::uint64_t generation,
    std::uint64_t revision,
    MediaRunningTime expectedDeadline,
    MediaRunningTime nextDeadline,
    std::uint64_t nextRandomStateValue) noexcept
    : m_generation(generation),
      m_revision(revision),
      m_expectedDeadline(expectedDeadline),
      m_nextDeadline(nextDeadline),
      m_nextRandomState(nextRandomStateValue)
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
    MediaRtcpReportingPolicy reportingPolicy,
    MediaRunningTime maximumLateness,
    std::uint64_t generation,
    std::uint64_t randomState) noexcept
    : m_nextDeadline(initialDeadline),
      m_reportingPolicy(std::move(reportingPolicy)),
      m_maximumLateness(maximumLateness),
      m_generation(generation),
      m_revision(1),
      m_randomState(randomState)
{
}

::media::Result<MediaRtcpSenderReportSchedule>
MediaRtcpSenderReportSchedule::create(
    MediaRunningTime activation,
    MediaRtcpReportingPolicy reportingPolicy,
    MediaRunningTime maximumLateness,
    std::uint64_t generation,
    std::uint64_t randomSeed) noexcept
{
    if (!validInstant(activation) || maximumLateness.nanoseconds() < 0 ||
        generation == 0 || randomSeed == 0) {
        return ::media::Result<MediaRtcpSenderReportSchedule>::failure(
            invalidSchedule("RTCP sender report schedule parameters are incomplete"));
    }
    const auto state = nextRandomState(randomSeed);
    auto interval = randomizedInterval(
        reportingPolicy.initialBaseInterval(), state);
    if (!interval) {
        return ::media::Result<MediaRtcpSenderReportSchedule>::failure(
            interval.error());
    }
    auto deadline = activation.checkedAdd(interval.value());
    if (!deadline) {
        return ::media::Result<MediaRtcpSenderReportSchedule>::failure(
            deadline.error());
    }
    return ::media::Result<MediaRtcpSenderReportSchedule>::success(
        MediaRtcpSenderReportSchedule(
            deadline.value(), std::move(reportingPolicy), maximumLateness,
            generation, state));
}

::media::Result<std::optional<MediaRtcpSenderReportScheduleDecision>>
MediaRtcpSenderReportSchedule::prepare(
    MediaRunningTime now,
    std::uint64_t generation) const noexcept
{
    using PrepareResult = ::media::Result<
        std::optional<MediaRtcpSenderReportScheduleDecision>>;
    if (!validInstant(now)) {
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
    const auto lateness = latenessResult.value();
    if (lateness > m_maximumLateness) {
        return PrepareResult::failure(
            invalidSchedule("RTCP sender report exceeded maximum lateness"));
    }

    const auto nextState = nextRandomState(m_randomState);
    auto interval = randomizedInterval(
        m_reportingPolicy.steadyBaseInterval(), nextState);
    if (!interval) return PrepareResult::failure(interval.error());
    auto nextDeadline = now.checkedAdd(interval.value());
    if (!nextDeadline) return PrepareResult::failure(nextDeadline.error());
    MediaRtcpSenderReportCommitToken token(
        m_generation, m_revision, m_nextDeadline, nextDeadline.value(),
        nextState);
    return PrepareResult::success(MediaRtcpSenderReportScheduleDecision(
        m_nextDeadline, now, nextDeadline.value(), lateness, 0,
        std::move(token)));
}

::media::Status MediaRtcpSenderReportSchedule::commit(
    const MediaRtcpSenderReportCommitToken& token) noexcept
{
    if (token.m_generation != m_generation ||
        token.m_revision != m_revision ||
        token.m_expectedDeadline != m_nextDeadline ||
        token.m_nextDeadline <= m_nextDeadline ||
        token.m_nextRandomState == 0 ||
        m_revision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            invalidSchedule(
                "RTCP sender report commit token is stale or invalid"));
    }
    m_nextDeadline = token.m_nextDeadline;
    m_randomState = token.m_nextRandomState;
    ++m_revision;
    return ::media::Status::success();
}

::media::Status MediaRtcpSenderReportSchedule::reset(
    MediaRunningTime activation,
    std::uint64_t generation,
    std::uint64_t randomSeed) noexcept
{
    if (!validInstant(activation) || generation <= m_generation ||
        randomSeed == 0 ||
        m_revision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            invalidSchedule(
                "RTCP sender report reset requires a newer generation, activation, and seed"));
    }
    const auto state = nextRandomState(randomSeed);
    auto interval = randomizedInterval(
        m_reportingPolicy.initialBaseInterval(), state);
    if (!interval) return ::media::Status::failure(interval.error());
    auto deadline = activation.checkedAdd(interval.value());
    if (!deadline) return ::media::Status::failure(deadline.error());
    m_nextDeadline = deadline.value();
    m_generation = generation;
    m_randomState = state;
    ++m_revision;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
