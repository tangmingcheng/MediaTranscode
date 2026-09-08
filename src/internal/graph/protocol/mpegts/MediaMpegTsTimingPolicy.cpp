#include "internal/graph/protocol/mpegts/MediaMpegTsTimingPolicy.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool valid(const MediaMpegTsTimingConstraint& constraint) noexcept
{
    return constraint.value.nanoseconds() > 0 &&
        !constraint.authority.empty() &&
        constraint.source != MediaMpegTsTimingConstraintSource::Unknown;
}

} // namespace

MediaMpegTsTimingPolicy::MediaMpegTsTimingPolicy(
    MediaMpegTsTimingConstraint pcrInterval,
    MediaMpegTsTimingConstraint maximumPcrGap,
    MediaMpegTsTimingConstraint psiRepeatInterval,
    MediaMpegTsTimingConstraint maximumReleaseJitter,
    int timestampTimeBaseNumerator,
    int timestampTimeBaseDenominator) noexcept
    : m_pcrInterval(std::move(pcrInterval)),
      m_maximumPcrGap(std::move(maximumPcrGap)),
      m_psiRepeatInterval(std::move(psiRepeatInterval)),
      m_maximumReleaseJitter(std::move(maximumReleaseJitter)),
      m_timestampTimeBaseNumerator(timestampTimeBaseNumerator),
      m_timestampTimeBaseDenominator(timestampTimeBaseDenominator)
{
}

::media::Result<MediaMpegTsTimingPolicy> MediaMpegTsTimingPolicy::create(
    MediaMpegTsTimingConstraint pcrInterval,
    MediaMpegTsTimingConstraint maximumPcrGap,
    MediaMpegTsTimingConstraint psiRepeatInterval,
    MediaMpegTsTimingConstraint maximumReleaseJitter,
    int timestampTimeBaseNumerator,
    int timestampTimeBaseDenominator)
{
    if (!valid(pcrInterval) || !valid(maximumPcrGap) ||
        !valid(psiRepeatInterval) || !valid(maximumReleaseJitter) ||
        maximumPcrGap.value <= pcrInterval.value ||
        psiRepeatInterval.value <= pcrInterval.value ||
        maximumReleaseJitter.value >= pcrInterval.value ||
        timestampTimeBaseNumerator <= 0 ||
        timestampTimeBaseDenominator <= 0) {
        return ::media::Result<MediaMpegTsTimingPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS timing policy requires authoritative ordered PCR, PSI, release-jitter, and clock constraints"));
    }
    return ::media::Result<MediaMpegTsTimingPolicy>::success(
        MediaMpegTsTimingPolicy(
            std::move(pcrInterval), std::move(maximumPcrGap),
            std::move(psiRepeatInterval), std::move(maximumReleaseJitter),
            timestampTimeBaseNumerator, timestampTimeBaseDenominator));
}

const MediaMpegTsTimingConstraint&
MediaMpegTsTimingPolicy::pcrInterval() const noexcept { return m_pcrInterval; }
const MediaMpegTsTimingConstraint&
MediaMpegTsTimingPolicy::maximumPcrGap() const noexcept { return m_maximumPcrGap; }
const MediaMpegTsTimingConstraint&
MediaMpegTsTimingPolicy::psiRepeatInterval() const noexcept { return m_psiRepeatInterval; }
const MediaMpegTsTimingConstraint&
MediaMpegTsTimingPolicy::maximumReleaseJitter() const noexcept { return m_maximumReleaseJitter; }

MediaTsOutputClockPolicy MediaMpegTsTimingPolicy::clockPolicy() const noexcept
{
    return MediaTsOutputClockPolicy{
        m_pcrInterval.value, m_maximumPcrGap.value,
        m_maximumReleaseJitter.value,
        m_timestampTimeBaseNumerator, m_timestampTimeBaseDenominator};
}

int MediaMpegTsTimingPolicy::timestampTimeBaseNumerator() const noexcept
{
    return m_timestampTimeBaseNumerator;
}

int MediaMpegTsTimingPolicy::timestampTimeBaseDenominator() const noexcept
{
    return m_timestampTimeBaseDenominator;
}

} // namespace media::ffmpeg::graph
