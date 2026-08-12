#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"

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
    if (request.parameters.queues.packet == 0 ||
        request.parameters.queues.packet > MediaAvStartupMaximumUnitCapacity ||
        !request.avSyncStartup.maximumVideoUnitBytes ||
        !request.avSyncStartup.maximumAudioUnitBytes ||
        !request.avSyncStartup.maximumGap ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0 ||
        *request.avSyncStartup.maximumAudioUnitBytes == 0 ||
        *request.avSyncStartup.maximumGap <= runningTime(0)) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup requires explicit unit and byte capacity inputs"));
    }

    MediaAvSyncStartupPolicy startup;
    startup.requireVideoKeyFrame = true;
    startup.trimAudioToCommonStart = true;
    startup.maximumWaitNs = runningTime(10 * Second);
    startup.prerollNs = runningTime(500 * Millisecond);
    startup.keyFrameWaitNs = runningTime(5 * Second);
    startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    startup.maximumGapNs = *request.avSyncStartup.maximumGap;
    startup.outputLeadNs = runningTime(100 * Millisecond);
    startup.videoCapacity = request.parameters.queues.packet;
    startup.audioCapacity = request.parameters.queues.packet;

    const auto units = static_cast<std::uint64_t>(
        request.parameters.queues.packet);
    const auto videoUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumVideoUnitBytes);
    const auto audioUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumAudioUnitBytes);
    if (units > std::numeric_limits<std::uint64_t>::max() / videoUnitBytes ||
        units > std::numeric_limits<std::uint64_t>::max() / audioUnitBytes) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup byte capacity is not representable"));
    }
    startup.videoByteCapacity = units * videoUnitBytes;
    startup.audioByteCapacity = units * audioUnitBytes;
    startup.maximumVideoUnitBytes = videoUnitBytes;
    startup.maximumAudioUnitBytes = audioUnitBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (videoUnitBytes > maximumSerialized ||
        audioUnitBytes > maximumSerialized ||
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
