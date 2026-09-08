#include "internal/graph/planner/realtime/MediaMpegTsOutputTimingPlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::int64_t ProtocolMaximumPcrGapNs = 100'000'000;
constexpr std::int64_t InteropMaximumPsiRepeatIntervalNs = 100'000'000;
constexpr int TimestampTimeBaseNumerator = 1;
constexpr int TimestampTimeBaseDenominator = 90'000;

constexpr const char* VariableBitratePcrAuthority =
    "FFmpeg MPEG-TS VBR automatic PCR frame-cadence policy";
constexpr const char* ProtocolPcrGapAuthority =
    "ISO/IEC 13818-1 PCR repetition maximum";
constexpr const char* InteropPsiAuthority =
    "FFmpeg MPEG-TS default PAT/PMT retransmission period";

::media::Result<MediaRunningTime> preparedVideoCadence(MediaRational cadence)
{
    if (cadence.num <= 0 || cadence.den <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output timing requires prepared video cadence"));
    }
    auto nanoseconds = MediaCheckedArithmetic::ceilScale(
        static_cast<std::uint64_t>(cadence.den), NanosecondsPerSecond,
        static_cast<std::uint64_t>(cadence.num),
        "MPEG-TS prepared video cadence");
    if (!nanoseconds || nanoseconds.value() == 0 ||
        nanoseconds.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRunningTime>::failure(
            nanoseconds ? ::media::ErrorInfo::invalidArgument(
                              "MPEG-TS prepared video cadence is outside the running-time range")
                        : nanoseconds.error());
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(nanoseconds.value())));
}

::media::Result<MediaRunningTime> variableBitratePcrInterval(
    MediaRational videoCadence)
{
    auto cadence = preparedVideoCadence(videoCadence);
    if (!cadence) {
        return cadence;
    }
    const auto frameNanoseconds = static_cast<std::uint64_t>(
        cadence.value().nanoseconds());
    const auto maximumExclusive = static_cast<std::uint64_t>(
        ProtocolMaximumPcrGapNs);
    const std::uint64_t framesPerPcr =
        (maximumExclusive - 1U) / frameNanoseconds;
    if (framesPerPcr == 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS VBR output cadence cannot form a whole-frame PCR interval below 100 ms"));
    }
    auto interval = MediaCheckedArithmetic::multiply(
        framesPerPcr, frameNanoseconds,
        "MPEG-TS VBR whole-frame PCR interval");
    if (!interval || interval.value() == 0 ||
        interval.value() >= maximumExclusive ||
        interval.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRunningTime>::failure(
            !interval ? interval.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS VBR PCR interval is outside the protocol range"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(interval.value())));
}

} // namespace

::media::Result<MediaMpegTsTimingPolicy>
MediaMpegTsOutputTimingPlanner::planVariableBitrate(
    MediaRunningTime maximumReleaseJitter,
    std::string releaseJitterAuthority,
    MediaRational videoCadence)
{
    auto pcrInterval = variableBitratePcrInterval(videoCadence);
    if (!pcrInterval || maximumReleaseJitter.nanoseconds() <= 0 ||
        releaseJitterAuthority.empty()) {
        return ::media::Result<MediaMpegTsTimingPolicy>::failure(
            !pcrInterval ? pcrInterval.error() :
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS timing requires release-jitter and prepared cadence authorities"));
    }
    return MediaMpegTsTimingPolicy::create(
        {pcrInterval.value(), VariableBitratePcrAuthority,
         MediaMpegTsTimingConstraintSource::PlannerDerivedMinimum},
        {MediaRunningTime::fromNanoseconds(ProtocolMaximumPcrGapNs),
         ProtocolPcrGapAuthority,
         MediaMpegTsTimingConstraintSource::ProtocolMaximum},
        {MediaRunningTime::fromNanoseconds(
             InteropMaximumPsiRepeatIntervalNs),
         InteropPsiAuthority,
         MediaMpegTsTimingConstraintSource::ProtocolMaximum},
        {maximumReleaseJitter, std::move(releaseJitterAuthority),
         MediaMpegTsTimingConstraintSource::DeploymentServiceSlo},
        TimestampTimeBaseNumerator, TimestampTimeBaseDenominator);
}

::media::Result<MediaRunningTime>
MediaMpegTsOutputTimingPlanner::startupEmissionPreroll(
    MediaRunningTime transportDecodeLead,
    MediaRational videoCadence,
    std::optional<MediaRunningTime> audioCadence,
    const MediaMpegTsTimingPolicy& timingPolicy)
{
    auto videoCadenceValue = preparedVideoCadence(videoCadence);
    if (transportDecodeLead.nanoseconds() <= 0 || !videoCadenceValue ||
        (audioCadence && audioCadence->nanoseconds() <= 0)) {
        return ::media::Result<MediaRunningTime>::failure(
            !videoCadenceValue ? videoCadenceValue.error() :
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS startup timing requires transport lead and prepared output cadences"));
    }
    auto required = (std::max)(
        videoCadenceValue.value(),
        timingPolicy.pcrInterval().value);
    if (audioCadence) {
        required = (std::max)(required, *audioCadence);
    }
    return ::media::Result<MediaRunningTime>::success(
        (std::min)(transportDecodeLead, required));
}

} // namespace media::ffmpeg::graph
