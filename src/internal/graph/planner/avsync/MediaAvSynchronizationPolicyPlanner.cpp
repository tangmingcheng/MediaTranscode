#include "internal/graph/planner/avsync/MediaAvSynchronizationPolicyPlanner.h"

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

} // namespace

void MediaAvSynchronizationPolicyPlanner::apply(MediaAvSyncPlan& plan)
{
    plan.masterClockMode = MediaAvSyncMasterClockMode::SteadyMonotonic;
    plan.canonicalTimeBaseNumerator = 1;
    plan.canonicalTimeBaseDenominator = 1'000'000'000;

    plan.video.earlyHoldThresholdNs = runningTime(20 * Millisecond);
    plan.video.lateDisplayThresholdNs = runningTime(40 * Millisecond);
    plan.video.dropThresholdNs = runningTime(80 * Millisecond);
    plan.video.allowRecoveryRepeat = true;
    plan.video.maximumConsecutiveRecoveryActions = 5;

    plan.recovery.hardDiscontinuityThresholdNs = runningTime(250 * Millisecond);

    plan.metrics.collectStateAndGeneration = true;
    plan.metrics.collectClockEvidence = true;
    plan.metrics.collectQueueDurations = true;
    plan.metrics.collectPhaseErrors = true;
    plan.metrics.collectAudioCorrection = true;
    plan.metrics.collectVideoRecoveryCounts = true;
    plan.metrics.collectDiscontinuityCounts = true;
    plan.metrics.collectProtocolClockHealth = true;
    plan.metrics.maximumStartupSkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumSteadyP95SkewNs = runningTime(20 * Millisecond);
    plan.metrics.maximumSteadyP99SkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumDriftNsPerHour = runningTime(Millisecond);
}

} // namespace media::ffmpeg::graph
