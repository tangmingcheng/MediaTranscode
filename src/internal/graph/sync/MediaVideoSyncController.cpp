#include "internal/graph/sync/MediaVideoSyncController.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <limits>
#include <optional>

namespace media::ffmpeg::graph {

MediaVideoSyncController::MediaVideoSyncController(
    MediaAvSyncTopology topology,
    Policy policy,
    std::uint64_t generation) noexcept
    : m_topology(topology)
    , m_policy(policy)
    , m_generation(generation)
{
}

MediaAvSyncResult<MediaVideoSyncController> MediaVideoSyncController::create(
    const MediaAvSyncPlan& plan,
    std::uint64_t generation)
{
    const auto planStatus = MediaAvSyncPlanValidator::validate(plan);
    if (!planStatus || generation == 0) {
        return MediaAvSyncResult<MediaVideoSyncController>::failure(
            MediaAvSyncError(
                MediaAvSyncErrorCode::InvalidVideoSyncPolicy,
                plan.topology.value_or(MediaAvSyncTopology::Unknown),
                MediaAvSyncErrorState::VideoSync,
                "create",
                "video",
                "video",
                generation,
                std::nullopt,
                std::nullopt,
                MediaRunningTime::fromNanoseconds(0),
                MediaRunningTime::fromNanoseconds(0),
                std::numeric_limits<std::int64_t>::min(),
                std::numeric_limits<std::int64_t>::max(),
                "incomplete or inconsistent planner-owned video sync policy"));
    }

    return MediaAvSyncResult<MediaVideoSyncController>::success(
        MediaVideoSyncController(
            *plan.topology,
            Policy{
                plan.video.earlyHoldThresholdNs->nanoseconds(),
                plan.video.lateDisplayThresholdNs->nanoseconds(),
                plan.video.dropThresholdNs->nanoseconds(),
                *plan.video.allowRecoveryRepeat,
                *plan.video.maximumConsecutiveRecoveryActions,
                plan.recovery.hardDiscontinuityThresholdNs->nanoseconds()},
            generation));
}

MediaAvSyncResult<MediaVideoSyncDecision> MediaVideoSyncController::update(
    const MediaVideoSyncMeasurement& measurement)
{
    if (const auto* frame = std::get_if<MediaVideoFrameMeasurement>(&measurement)) {
        return updateFrame(*frame);
    }
    return updateRepeat(std::get<MediaVideoRepeatRequest>(measurement));
}

MediaAvSyncResult<MediaVideoSyncDecision> MediaVideoSyncController::updateFrame(
    const MediaVideoFrameMeasurement& measurement)
{
    if (measurement.generation != m_generation) {
        return isolatedGenerationDecision(
            measurement.targetPresentationOnMaster,
            MediaRunningTime::fromNanoseconds(0),
            measurement.generation,
            measurement.sequence);
    }
    if (measurement.sequence == 0) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_frame",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  "video sync sequence must be positive"));
    }

    auto phase = measurement.targetPresentationOnMaster.checkedSubtract(
        measurement.masterNow);
    if (!phase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "measure_frame",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  phase.error().message.c_str()));
    }
    if (auto status = validateSequence(measurement.sequence); !status) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(status.error());
    }

    const std::int64_t errorNs = phase.value().nanoseconds();
    const std::int64_t hard = m_policy.hardDiscontinuityThresholdNs;
    if (errorNs >= hard || errorNs <= -hard) {
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     measurement.targetPresentationOnMaster,
                     phase.value(),
                     measurement.sequence));
    }
    if (errorNs > m_policy.earlyHoldThresholdNs) {
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Hold,
                     measurement.targetPresentationOnMaster,
                     phase.value(),
                     measurement.sequence));
    }
    if (errorNs >= -m_policy.lateDisplayThresholdNs) {
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Display,
                     measurement.targetPresentationOnMaster,
                     phase.value(),
                     measurement.sequence));
    }
    if (errorNs > -m_policy.dropThresholdNs) {
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::DisplayLate,
                     measurement.targetPresentationOnMaster,
                     phase.value(),
                     measurement.sequence));
    }
    if (measurement.keyFrame) {
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::DisplayPreservedKeyFrame,
                     measurement.targetPresentationOnMaster,
                     phase.value(),
                     measurement.sequence));
    }
    return recoveryDecision(MediaVideoSyncDecisionKind::Drop,
                            measurement.targetPresentationOnMaster,
                            phase.value(),
                            measurement.sequence);
}

MediaAvSyncResult<MediaVideoSyncDecision> MediaVideoSyncController::updateRepeat(
    const MediaVideoRepeatRequest& request)
{
    if (request.generation != m_generation) {
        return isolatedGenerationDecision(
            request.repeatPresentationOnMaster,
            MediaRunningTime::fromNanoseconds(0),
            request.generation,
            request.sequence);
    }
    if (request.sequence == 0) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  "video sync sequence must be positive"));
    }


    auto phase = request.repeatPresentationOnMaster.checkedSubtract(request.masterNow);
    if (!phase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  phase.error().message.c_str()));
    }
    if (auto status = validateSequence(request.sequence); !status) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(status.error());
    }
    if (!(request.lastEmittedPresentationOnMaster <
              request.repeatPresentationOnMaster) ||
        request.repeatPresentationOnMaster > request.masterNow) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  "repeat request requires last emitted < repeat slot <= master now"));
    }

    const std::int64_t errorNs = phase.value().nanoseconds();
    const std::int64_t hard = m_policy.hardDiscontinuityThresholdNs;
    if (errorNs >= hard || errorNs <= -hard) {
        m_lastSequence = request.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     request.repeatPresentationOnMaster,
                     phase.value(),
                     request.sequence));
    }
    if (!m_policy.allowRecoveryRepeat) {
        m_lastSequence = request.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::NoAction,
                     request.repeatPresentationOnMaster,
                     phase.value(),
                     request.sequence));
    }
    return recoveryDecision(MediaVideoSyncDecisionKind::RepeatPreviousFrame,
                            request.repeatPresentationOnMaster,
                            phase.value(),
                            request.sequence);
}

MediaAvSyncResult<MediaVideoSyncDecision>
MediaVideoSyncController::recoveryDecision(
    MediaVideoSyncDecisionKind kind,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime phaseError,
    std::uint64_t sequence)
{
    m_lastSequence = sequence;
    if (m_consecutiveRecoveryActions >=
        m_policy.maximumConsecutiveRecoveryActions) {
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     presentationOnMaster,
                     phaseError,
                     sequence));
    }
    ++m_consecutiveRecoveryActions;
    return MediaAvSyncResult<MediaVideoSyncDecision>::success(
        decision(kind, presentationOnMaster, phaseError, sequence));
}

MediaAvSyncResult<MediaVideoSyncDecision>
MediaVideoSyncController::isolatedGenerationDecision(
    MediaRunningTime presentationOnMaster,
    MediaRunningTime phaseError,
    std::uint64_t observedGeneration,
    std::uint64_t sequence) const
{
    const auto kind = observedGeneration < m_generation
        ? MediaVideoSyncDecisionKind::DropOldGeneration
        : MediaVideoSyncDecisionKind::Reacquire;
    return MediaAvSyncResult<MediaVideoSyncDecision>::success(
        MediaVideoSyncDecision(
            kind, presentationOnMaster, phaseError,
            observedGeneration, sequence, m_consecutiveRecoveryActions));
}

MediaAvSyncStatus MediaVideoSyncController::validateSequence(
    std::uint64_t sequence) const
{
    if (sequence == 0 || sequence <= m_lastSequence) {
        return MediaAvSyncStatus::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "validate_sequence",
                  m_generation,
                  MediaRunningTime::fromNanoseconds(0),
                  "video sync sequence must be positive and strictly increasing"));
    }
    return MediaAvSyncStatus::success();
}

MediaAvSyncStatus MediaVideoSyncController::reset(std::uint64_t generation)
{
    if (generation == 0 || generation <= m_generation) {
        return MediaAvSyncStatus::failure(
            error(MediaAvSyncErrorCode::InvalidGenerationTransition,
                  "reset",
                  generation,
                  MediaRunningTime::fromNanoseconds(0),
                  "video sync reset requires a strictly increasing generation"));
    }
    m_generation = generation;
    m_lastSequence = 0;
    m_consecutiveRecoveryActions = 0;
    return MediaAvSyncStatus::success();
}

MediaAvSyncError MediaVideoSyncController::error(
    MediaAvSyncErrorCode code,
    const char* operation,
    std::uint64_t observedGeneration,
    MediaRunningTime presentationOnMaster,
    const char* detail) const
{
    return MediaAvSyncError(
        code,
        m_topology,
        MediaAvSyncErrorState::VideoSync,
        operation,
        "video",
        "video",
        m_generation,
        observedGeneration,
        presentationOnMaster,
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
        detail);
}

MediaVideoSyncDecision MediaVideoSyncController::decision(
    MediaVideoSyncDecisionKind kind,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime phaseError,
    std::uint64_t sequence) const noexcept
{
    return MediaVideoSyncDecision(
        kind, presentationOnMaster, phaseError,
        m_generation, sequence, m_consecutiveRecoveryActions);
}

} // namespace media::ffmpeg::graph
