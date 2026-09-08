#include "internal/graph/sync/MediaVideoSyncController.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <limits>
#include <optional>

namespace media::ffmpeg::graph {

MediaVideoSyncController::MediaVideoSyncController(
    MediaAvSyncSourceClockMode sourceClockMode,
    Policy policy,
    std::uint64_t generation) noexcept
    : m_sourceClockMode(sourceClockMode)
    , m_policy(policy)
    , m_generation(generation)
{
}

MediaAvSyncResult<MediaVideoSyncController> MediaVideoSyncController::create(
    const MediaAvSyncPlan& plan,
    std::uint64_t generation)
{
    const auto planStatus = MediaAvSyncPlanValidator::validateRuntime(plan);
    if (!planStatus || generation == 0) {
        return MediaAvSyncResult<MediaVideoSyncController>::failure(
            MediaAvSyncError(
                MediaAvSyncErrorCode::InvalidVideoSyncPolicy,
                plan.sourceClockMode,
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
            *plan.sourceClockMode,
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
    if (auto status = validateFrameIdentity(measurement); !status) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(status.error());
    }
    if (measurement.observedAtMaster >
        measurement.decisionHorizonOnMaster) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_frame",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  "video observation exceeds its decision horizon"));
    }

    auto presentationPhase = measurement.targetPresentationOnMaster.checkedSubtract(
        measurement.observedAtMaster);
    if (!presentationPhase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "measure_frame",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  presentationPhase.error().message.c_str()));
    }
    auto dispatchPhase = measurement.dispatchOnMaster.checkedSubtract(
        measurement.decisionHorizonOnMaster);
    if (!dispatchPhase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "dispatch_deadline",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  dispatchPhase.error().message.c_str()));
    }
    const std::int64_t errorNs = presentationPhase.value().nanoseconds();
    const std::int64_t hard = m_policy.hardDiscontinuityThresholdNs;
    if (errorNs <= -hard) {
        m_heldFrame.reset();
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     measurement.targetPresentationOnMaster,
                     presentationPhase.value(),
                     measurement.sequence,
                     MediaVideoReacquisitionCause::HardPhaseError));
    }
    if (dispatchPhase.value().nanoseconds() > m_policy.earlyHoldThresholdNs) {
        auto recheckAt = measurement.dispatchOnMaster.checkedSubtract(
            MediaRunningTime::fromNanoseconds(m_policy.earlyHoldThresholdNs));
        if (!recheckAt) {
            return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
                error(MediaAvSyncErrorCode::TimeOverflow,
                      "hold_deadline",
                      measurement.generation,
                      measurement.targetPresentationOnMaster,
                      recheckAt.error().message.c_str()));
        }
        m_heldFrame = HeldFrameIdentity{measurement.sequence,
                                        measurement.dispatchOnMaster,
                                        measurement.targetPresentationOnMaster,
                                        measurement.keyFrame};
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            MediaVideoSyncDecision(MediaVideoSyncDecisionKind::Hold,
                                   measurement.targetPresentationOnMaster,
                                   presentationPhase.value(),
                                   m_generation,
                                   measurement.sequence,
                                   m_consecutiveRecoveryActions,
                                   recheckAt.value()));
    }
    if (errorNs >= -m_policy.lateDisplayThresholdNs) {
        m_heldFrame.reset();
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Display,
                     measurement.targetPresentationOnMaster,
                     presentationPhase.value(),
                     measurement.sequence));
    }
    if (errorNs > -m_policy.dropThresholdNs) {
        m_heldFrame.reset();
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::DisplayLate,
                     measurement.targetPresentationOnMaster,
                     presentationPhase.value(),
                     measurement.sequence));
    }
    if (measurement.keyFrame) {
        m_heldFrame.reset();
        m_lastSequence = measurement.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::DisplayPreservedKeyFrame,
                     measurement.targetPresentationOnMaster,
                     presentationPhase.value(),
                     measurement.sequence));
    }
    return recoveryDecision(MediaVideoSyncDecisionKind::Drop,
                            measurement.targetPresentationOnMaster,
                            presentationPhase.value(),
                            measurement.sequence);
}

MediaAvSyncResult<MediaVideoSyncDecision> MediaVideoSyncController::updateRepeat(
    const MediaVideoRepeatRequest& request)
{
    if (m_heldFrame && request.generation == m_generation) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  "repeat cannot overtake a held video frame"));
    }
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
    if (request.observedAtMaster > request.decisionHorizonOnMaster) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  "video repeat observation exceeds its decision horizon"));
    }

    auto presentationPhase = request.repeatPresentationOnMaster.checkedSubtract(
        request.observedAtMaster);
    if (!presentationPhase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  presentationPhase.error().message.c_str()));
    }
    auto dispatchPhase = request.repeatDispatchOnMaster.checkedSubtract(
        request.decisionHorizonOnMaster);
    if (!dispatchPhase) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::TimeOverflow,
                  "repeat_dispatch_deadline",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  dispatchPhase.error().message.c_str()));
    }
    if (auto status = validateSequence(request.sequence); !status) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(status.error());
    }
    if (!(request.lastEmittedPresentationOnMaster <
              request.repeatPresentationOnMaster)) {
        return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "measure_repeat",
                  request.generation,
                  request.repeatPresentationOnMaster,
                  "repeat request requires last emitted presentation < repeat presentation"));
    }

    const std::int64_t errorNs = presentationPhase.value().nanoseconds();
    const std::int64_t hard = m_policy.hardDiscontinuityThresholdNs;
    if (errorNs <= -hard) {
        m_lastSequence = request.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     request.repeatPresentationOnMaster,
                     presentationPhase.value(),
                     request.sequence,
                     MediaVideoReacquisitionCause::HardPhaseError));
    }
    if (dispatchPhase.value().nanoseconds() > m_policy.earlyHoldThresholdNs) {
        auto recheckAt = request.repeatDispatchOnMaster.checkedSubtract(
            MediaRunningTime::fromNanoseconds(m_policy.earlyHoldThresholdNs));
        if (!recheckAt) {
            return MediaAvSyncResult<MediaVideoSyncDecision>::failure(
                error(MediaAvSyncErrorCode::TimeOverflow,
                      "repeat_hold_deadline",
                      request.generation,
                      request.repeatPresentationOnMaster,
                      recheckAt.error().message.c_str()));
        }
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            MediaVideoSyncDecision(
                MediaVideoSyncDecisionKind::Hold,
                request.repeatPresentationOnMaster,
                presentationPhase.value(),
                m_generation,
                request.sequence,
                m_consecutiveRecoveryActions,
                recheckAt.value()));
    }
    if (!m_policy.allowRecoveryRepeat) {
        m_lastSequence = request.sequence;
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::NoAction,
                     request.repeatPresentationOnMaster,
                     presentationPhase.value(),
                     request.sequence));
    }
    return recoveryDecision(MediaVideoSyncDecisionKind::RepeatPreviousFrame,
                            request.repeatPresentationOnMaster,
                            presentationPhase.value(),
                            request.sequence);
}

MediaAvSyncResult<MediaVideoSyncDecision>
MediaVideoSyncController::recoveryDecision(
    MediaVideoSyncDecisionKind kind,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime phaseError,
    std::uint64_t sequence)
{
    m_heldFrame.reset();
    m_lastSequence = sequence;
    if (m_consecutiveRecoveryActions >=
        m_policy.maximumConsecutiveRecoveryActions) {
        m_consecutiveRecoveryActions = 0;
        return MediaAvSyncResult<MediaVideoSyncDecision>::success(
            decision(MediaVideoSyncDecisionKind::Reacquire,
                     presentationOnMaster,
                     phaseError,
                     sequence,
                     MediaVideoReacquisitionCause::RecoveryBudgetExhausted));
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
            observedGeneration, sequence, m_consecutiveRecoveryActions,
            std::nullopt,
            kind == MediaVideoSyncDecisionKind::Reacquire
                ? std::optional<MediaVideoReacquisitionCause>{
                      MediaVideoReacquisitionCause::GenerationMismatch}
                : std::optional<MediaVideoReacquisitionCause>{}));
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

MediaAvSyncStatus MediaVideoSyncController::validateFrameIdentity(
    const MediaVideoFrameMeasurement& measurement) const
{
    if (!m_heldFrame) return validateSequence(measurement.sequence);
    if (measurement.sequence != m_heldFrame->sequence ||
        measurement.dispatchOnMaster != m_heldFrame->dispatchOnMaster ||
        measurement.targetPresentationOnMaster !=
            m_heldFrame->targetPresentationOnMaster ||
        measurement.keyFrame != m_heldFrame->keyFrame) {
        return MediaAvSyncStatus::failure(
            error(MediaAvSyncErrorCode::InvalidVideoSyncMeasurement,
                  "validate_held_frame",
                  measurement.generation,
                  measurement.targetPresentationOnMaster,
                  "held video identity is immutable until a terminal decision"));
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
    m_heldFrame.reset();
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
        m_sourceClockMode,
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
    std::uint64_t sequence,
    std::optional<MediaVideoReacquisitionCause> reacquisitionCause) const noexcept
{
    return MediaVideoSyncDecision(
        kind, presentationOnMaster, phaseError,
        m_generation, sequence, m_consecutiveRecoveryActions,
        std::nullopt, reacquisitionCause);
}

} // namespace media::ffmpeg::graph
