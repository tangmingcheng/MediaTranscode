#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
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

::media::Result<MediaAvSyncStartupPolicy> makePolicy(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeMediaCapacityPlan& capacity)
{
    if (!request.deployment || !capacity.audioUnits ||
        !capacity.audioUnitBytes || !capacity.audioBytes ||
        capacity.videoUnits == 0 || capacity.videoUnitBytes == 0 ||
        capacity.videoBytes == 0 ||
        capacity.videoUnits > MediaAvStartupMaximumUnitCapacity ||
        *capacity.audioUnits > MediaAvStartupMaximumUnitCapacity) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup deployment budget is incomplete"));
    }

    MediaAvSyncStartupPolicy startup;
    startup.requireVideoKeyFrame = true;
    startup.trimAudioToCommonStart = true;
    startup.maximumWaitNs = runningTime(10 * Second);
    startup.prerollNs = runningTime(500 * Millisecond);
    startup.keyFrameWaitNs = runningTime(5 * Second);
    startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    startup.maximumGapNs = capacity.maximumGap;
    startup.outputLeadNs =
        request.deployment->encode().latency.maximumResidence;
    startup.videoCapacity = capacity.videoUnits;
    startup.audioCapacity = *capacity.audioUnits;
    startup.videoByteCapacity = capacity.videoBytes;
    startup.audioByteCapacity = *capacity.audioBytes;
    startup.maximumVideoUnitBytes = capacity.videoUnitBytes;
    startup.maximumAudioUnitBytes = *capacity.audioUnitBytes;
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

} // namespace

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(ledger);
    if (!capacity) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            capacity.error());
    }
    return makePolicy(request, capacity.value());
}

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::planInputPreflight(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.deployment ||
        request.parameters.execution.streamSet !=
            MediaTranscodeStreamSet::AudioVideo) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V input observation requires deployment and stream-set facts"));
    }
    const auto graphBudget =
        request.deployment->encode().resources.maximumGraphMemoryBytes;
    const auto perStreamBudget = graphBudget / 2U;
    if (perStreamBudget == 0) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V input observation graph budget cannot admit both streams"));
    }
    return makePolicy(request, MediaRealtimeMediaCapacityPlan{
        MediaAvStartupMaximumUnitCapacity, perStreamBudget,
        perStreamBudget,
        MediaAvStartupMaximumUnitCapacity, perStreamBudget,
        perStreamBudget,
        request.deployment->encode().latency.maximumResidence});
}

} // namespace media::ffmpeg::graph
