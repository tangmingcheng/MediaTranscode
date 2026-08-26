#include "internal/graph/planner/realtime/MediaTsReceiverTimingPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"

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

constexpr const char* ProtocolPcrGapAuthority =
    "ISO/IEC 13818-1 PCR repetition maximum";
constexpr const char* InteropPsiAuthority =
    "managed MPEG-TS receiver PAT/PMT acquisition profile";

::media::Result<MediaRunningTime> preparedVideoCadence(MediaRational cadence)
{
    if (cadence.num <= 0 || cadence.den <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::notInitialized(
                "TS receiver timing requires prepared video cadence"));
    }
    auto nanoseconds = MediaRealtimePlanningArithmetic::ceilScale(
        static_cast<std::uint64_t>(cadence.den), NanosecondsPerSecond,
        static_cast<std::uint64_t>(cadence.num),
        "TS prepared video cadence");
    if (!nanoseconds || nanoseconds.value() == 0 ||
        nanoseconds.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRunningTime>::failure(
            nanoseconds ? ::media::ErrorInfo::invalidArgument(
                              "TS prepared video cadence is outside the running-time range")
                        : nanoseconds.error());
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(nanoseconds.value())));
}

} // namespace

::media::Result<MediaMpegTsTimingPolicy>
MediaTsReceiverTimingPlanner::plan(
    MediaRunningTime receiverTransportDecodeLead,
    std::string receiverAuthority,
    MediaRunningTime targetResidence,
    MediaRunningTime maximumResidence,
    MediaRunningTime maximumReleaseJitter,
    std::string releaseJitterAuthority,
    MediaRational videoCadence,
    std::optional<MediaRunningTime> audioCadence)
{
    auto video = preparedVideoCadence(videoCadence);
    if (!video || receiverTransportDecodeLead.nanoseconds() <= 0 ||
        receiverAuthority.empty() || targetResidence.nanoseconds() <= 0 ||
        maximumResidence < targetResidence ||
        maximumReleaseJitter.nanoseconds() <= 0 ||
        releaseJitterAuthority.empty() ||
        (audioCadence && audioCadence->nanoseconds() <= 0)) {
        return ::media::Result<MediaMpegTsTimingPolicy>::failure(
            !video ? video.error() : ::media::ErrorInfo::notInitialized(
                "MPEG-TS timing requires receiver, latency, release-jitter, and prepared cadence authorities"));
    }
    auto pcrInterval = (std::min)(receiverTransportDecodeLead,
        (std::min)(targetResidence, video.value()));
    if (audioCadence) pcrInterval = (std::min)(pcrInterval, *audioCadence);
    const auto maximumPcrGap = (std::min)(
        MediaRunningTime::fromNanoseconds(ProtocolMaximumPcrGapNs),
        (std::min)(receiverTransportDecodeLead, maximumResidence));
    const auto psiRepeat = (std::min)(
        MediaRunningTime::fromNanoseconds(InteropMaximumPsiRepeatIntervalNs),
        (std::min)(receiverTransportDecodeLead, maximumResidence));
    return MediaMpegTsTimingPolicy::create(
        {pcrInterval,
         "minimum(receiver=" + receiverAuthority +
             ",service-target,prepared-cadence)",
         MediaMpegTsTimingConstraintSource::PlannerDerivedMinimum},
        {maximumPcrGap,
         std::string(ProtocolPcrGapAuthority) + "+" + receiverAuthority,
         MediaMpegTsTimingConstraintSource::ProtocolMaximum},
        {psiRepeat,
         std::string(InteropPsiAuthority) + "+" + receiverAuthority,
         MediaMpegTsTimingConstraintSource::ProtocolMaximum},
        {maximumReleaseJitter, std::move(releaseJitterAuthority),
         MediaMpegTsTimingConstraintSource::DeploymentServiceSlo},
        TimestampTimeBaseNumerator, TimestampTimeBaseDenominator);
}

::media::Result<MediaRunningTime>
MediaTsReceiverTimingPlanner::startupEmissionPreroll(
    MediaRunningTime receiverTransportDecodeLead,
    MediaRational videoCadence,
    std::optional<MediaRunningTime> audioCadence,
    const MediaMpegTsTimingPolicy& timingPolicy)
{
    auto videoCadenceValue = preparedVideoCadence(videoCadence);
    if (receiverTransportDecodeLead.nanoseconds() <= 0 || !videoCadenceValue ||
        (audioCadence && audioCadence->nanoseconds() <= 0)) {
        return ::media::Result<MediaRunningTime>::failure(
            !videoCadenceValue ? videoCadenceValue.error() :
            ::media::ErrorInfo::notInitialized(
                "TS receiver timing requires lead and prepared output cadences"));
    }
    auto required = (std::max)(
        videoCadenceValue.value(),
        timingPolicy.pcrInterval().value);
    if (audioCadence) required = (std::max)(required, *audioCadence);
    return ::media::Result<MediaRunningTime>::success(
        (std::min)(receiverTransportDecodeLead, required));
}

} // namespace media::ffmpeg::graph
