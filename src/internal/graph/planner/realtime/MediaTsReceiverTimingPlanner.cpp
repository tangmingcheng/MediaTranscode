#include "internal/graph/planner/realtime/MediaTsReceiverTimingPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

} // namespace

::media::Result<MediaRunningTime>
MediaTsReceiverTimingPlanner::startupEmissionPreroll(
    MediaRunningTime receiverTransportDecodeLead,
    MediaRational videoCadence,
    std::optional<MediaRunningTime> audioCadence)
{
    if (videoCadence.num <= 0 || videoCadence.den <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::notInitialized(
                "TS receiver timing requires prepared video cadence"));
    }
    auto videoNanoseconds = MediaRealtimePlanningArithmetic::ceilScale(
        static_cast<std::uint64_t>(videoCadence.den),
        NanosecondsPerSecond,
        static_cast<std::uint64_t>(videoCadence.num),
        "TS prepared video cadence");
    if (receiverTransportDecodeLead.nanoseconds() <= 0 || !videoNanoseconds ||
        videoNanoseconds.value() == 0 ||
        videoNanoseconds.value() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        (audioCadence && audioCadence->nanoseconds() <= 0)) {
        return ::media::Result<MediaRunningTime>::failure(
            !videoNanoseconds ? videoNanoseconds.error() :
            ::media::ErrorInfo::notInitialized(
                "TS receiver timing requires lead and prepared output cadences"));
    }
    const auto video = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(videoNanoseconds.value()));
    auto required = (std::max)(
        video,
        pcrInterval());
    if (audioCadence) required = (std::max)(required, *audioCadence);
    return ::media::Result<MediaRunningTime>::success(
        (std::min)(receiverTransportDecodeLead, required));
}

} // namespace media::ffmpeg::graph
