#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;
constexpr std::int64_t Second = 1'000'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

} // namespace

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(request);
    if (!capacity || !capacity.value().audioUnits ||
        !capacity.value().audioUnitBytes || !capacity.value().audioBytes ||
        capacity.value().videoUnits > MediaAvStartupMaximumUnitCapacity) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            capacity ? ::media::ErrorInfo::invalidArgument(
                           "A/V startup deployment budget is incomplete")
                     : capacity.error());
    }

    MediaAvSyncStartupPolicy startup;
    startup.requireVideoKeyFrame = true;
    startup.trimAudioToCommonStart = true;
    startup.maximumWaitNs = runningTime(10 * Second);
    startup.prerollNs = runningTime(500 * Millisecond);
    startup.keyFrameWaitNs = runningTime(5 * Second);
    startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    startup.maximumGapNs = capacity.value().maximumGap;
    startup.outputLeadNs = runningTime(100 * Millisecond);
    startup.videoCapacity = capacity.value().videoUnits;
    startup.audioCapacity = *capacity.value().audioUnits;
    startup.videoByteCapacity = capacity.value().videoBytes;
    startup.audioByteCapacity = *capacity.value().audioBytes;
    startup.maximumVideoUnitBytes = capacity.value().videoUnitBytes;
    startup.maximumAudioUnitBytes = *capacity.value().audioUnitBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (*startup.maximumVideoUnitBytes > maximumSerialized ||
        *startup.maximumAudioUnitBytes > maximumSerialized ||
        *startup.videoByteCapacity > maximumSerialized ||
        *startup.audioByteCapacity > maximumSerialized) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup capacity exceeds the runtime option range"));
    }
    startup.allowDegradedClock = false;
    return ::media::Result<MediaAvSyncStartupPolicy>::success(
        std::move(startup));
}

} // namespace media::ffmpeg::graph
