#include "internal/graph/planner/realtime/MediaRealtimeVideoJoinWaitPlanner.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaRealtimeVideoJoinWaitPlan> MediaRealtimeVideoJoinWaitPlanner::plan(
    const MediaPreparedVideoRandomAccessEnvelope& randomAccess,
    MediaVideoJoinEncoderState state, MediaRealtimeVideoRuntimePlan& runtime,
    MediaRunningTime remainingFirstOutputBudget)
{
    using Result = ::media::Result<MediaRealtimeVideoJoinWaitPlan>;
    const auto activationLead = runtime.scheduling.activationLead;
    if ((state != MediaVideoJoinEncoderState::NewlyOpened && state != MediaVideoJoinEncoderState::AlreadyRunning) ||
        randomAccess.maximumIdrDistanceFrames == 0 || randomAccess.authority.empty() ||
        randomAccess.outputCadence.num <= 0 || randomAccess.outputCadence.den <= 0 ||
        activationLead.nanoseconds() < 0 ||
        (state == MediaVideoJoinEncoderState::NewlyOpened && !randomAccess.firstSubmittedFrameIsIdr))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Dynamic video join requires a proven finite IDR cadence"));
    const auto frames = state == MediaVideoJoinEncoderState::NewlyOpened
        ? std::uint64_t{1} : randomAccess.maximumIdrDistanceFrames;
    auto scale = MediaCheckedArithmetic::multiply(
        static_cast<std::uint64_t>(randomAccess.outputCadence.den), 1'000'000'000,
        "IDR cadence nanosecond scale");
    if (!scale) return Result::failure(scale.error());
    auto interval = MediaCheckedArithmetic::ceilScale(frames, scale.value(),
        static_cast<std::uint64_t>(randomAccess.outputCadence.num), "maximum natural IDR media interval");
    if (!interval || interval.value() > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
        return Result::failure(interval ? ::media::ErrorInfo::invalidArgument("IDR interval is not representable") : interval.error());
    const auto idr = MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(interval.value()));
    // activationLead already includes the protocol preparation/decode lead.
    // This checks nominal media feasibility, not unproven driver/CPU WCET.
    auto required = idr.checkedAdd(activationLead);
    if (!required) return Result::failure(required.error());
    MediaRealtimeVideoJoinWaitPlan result{idr, required.value(), remainingFirstOutputBudget};
    if (auto status = validateAdmission(result, remainingFirstOutputBudget); !status)
        return Result::failure(status.error());
    runtime.startup.maximumWait = result.maximumWait;
    return Result::success(result);
}

::media::Status MediaRealtimeVideoJoinWaitPlanner::validateAdmission(
    const MediaRealtimeVideoJoinWaitPlan& plan, MediaRunningTime remainingFirstOutputBudget)
{
    if (plan.maximumIdrInterval.nanoseconds() <= 0 ||
        plan.minimumFirstOutputBudget < plan.maximumIdrInterval ||
        plan.maximumWait < plan.minimumFirstOutputBudget ||
        remainingFirstOutputBudget <= plan.minimumFirstOutputBudget)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Dynamic video join IDR interval and activation lead do not fit the remaining first-output transaction budget"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
