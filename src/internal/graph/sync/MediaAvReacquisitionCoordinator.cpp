#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaAvReacquisitionCoordinator::MediaAvReacquisitionCoordinator(
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants)
    : m_transitionService(std::move(transition))
    , m_clock(std::move(clock))
    , m_participants(std::move(participants))
{
}

::media::Result<std::unique_ptr<MediaAvReacquisitionCoordinator>>
MediaAvReacquisitionCoordinator::create(
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants)
{
    if (!transition || !clock || participants.empty()) {
        return ::media::Result<
            std::unique_ptr<MediaAvReacquisitionCoordinator>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V reacquisition coordinator requires transition, clock, and participants"));
    }
    return ::media::Result<
        std::unique_ptr<MediaAvReacquisitionCoordinator>>::success(
        std::unique_ptr<MediaAvReacquisitionCoordinator>(
            new MediaAvReacquisitionCoordinator(
                std::move(transition),
                std::move(clock),
                std::move(participants))));
}

bool MediaAvReacquisitionCoordinator::matchesActiveRequest(
    const MediaAvReacquisitionRequest& request) const noexcept
{
    return m_request && *m_request == request;
}

bool MediaAvReacquisitionCoordinator::matchesTransition(
    std::uint64_t generation,
    std::uint64_t transitionSequence) const noexcept
{
    return m_transition &&
        m_transition->nextGeneration == generation &&
        m_transition->transitionSequence == transitionSequence;
}

::media::Status MediaAvReacquisitionCoordinator::failTerminalLocked(
    ::media::ErrorInfo error)
{
    if (!m_firstError) {
        auto failed =
            m_transitionService->failReacquisition(std::move(error));
        m_firstError = failed.error();
    }
    m_phase = MediaAvReacquisitionPhase::Aborted;
    m_inFlightTransitionSequence.reset();
    return ::media::Status::failure(*m_firstError);
}

::media::Status
MediaAvReacquisitionCoordinator::validateAndQueueRequest(
    MediaAvReacquisitionRequest request)
{
    const auto active = m_transitionService->snapshot();
    if (active.poisoned) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "A/V reacquisition requires a live epoch transition service"));
    }
    if (active.readiness != MediaAvGenerationReadiness::Locked ||
        !active.playbackEpoch || request.observedGeneration == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V reacquisition requires an active locked playback epoch"));
    }

    const std::uint64_t activeGeneration =
        active.playbackEpoch->generation;
    const bool future =
        request.reason == MediaAvReacquisitionReason::FutureGeneration;
    if (future && request.observedGeneration <= activeGeneration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Future-generation reacquisition requires a strictly newer observation"));
    }
    if (!future &&
        (request.observedGeneration != activeGeneration ||
         activeGeneration == std::numeric_limits<std::uint64_t>::max())) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Same-generation reacquisition requires the active non-exhausted generation"));
    }
    if (!m_request ||
        request.observedGeneration > m_request->observedGeneration) {
        m_request = request;
    }
    return ::media::Status::success();
}

::media::Status MediaAvReacquisitionCoordinator::observe(
    MediaAvReacquisitionRequest request)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_phase == MediaAvReacquisitionPhase::Inactive) {
        auto queued = validateAndQueueRequest(request);
        return queued ? queued : failTerminalLocked(queued.error());
    }
    const bool plannedNextGeneration =
        request.reason == MediaAvReacquisitionReason::FutureGeneration &&
        m_transition &&
        request.observedGeneration == m_transition->nextGeneration;
    if (matchesActiveRequest(request) || plannedNextGeneration) {
        return ::media::Status::success();
    }
    return failTerminalLocked(::media::ErrorInfo::invalidArgument(
        "A/V reacquisition rejects incompatible generation evidence after transition begin"));
}

::media::Status MediaAvReacquisitionCoordinator::request(
    MediaAvReacquisitionRequest request)
{
    MediaAvGenerationPurge purgeWork{};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_firstError) {
            return ::media::Status::failure(*m_firstError);
        }
        if (m_phase != MediaAvReacquisitionPhase::Inactive) {
            if (matchesActiveRequest(request)) {
                return ::media::Status::success();
            }
            return failTerminalLocked(::media::ErrorInfo::invalidArgument(
                "A/V reacquisition rejects an incompatible request after transition begin"));
        }

        auto queued = validateAndQueueRequest(request);
        if (!queued) return failTerminalLocked(queued.error());
        request = *m_request;
        const auto active = m_transitionService->snapshot();
        if (active.poisoned ||
            active.readiness != MediaAvGenerationReadiness::Locked ||
            !active.playbackEpoch) {
            return failTerminalLocked(::media::ErrorInfo::notInitialized(
                "A/V reacquisition lost its active locked playback epoch"));
        }
        const std::uint64_t oldGeneration =
            active.playbackEpoch->generation;
        const bool future =
            request.reason == MediaAvReacquisitionReason::FutureGeneration;
        const std::uint64_t nextGeneration =
            future ? request.observedGeneration : oldGeneration + 1;

        auto beganAt = m_clock->now();
        if (!beganAt) return failTerminalLocked(beganAt.error());
        auto purge = m_transitionService->beginReacquisition(
            oldGeneration, nextGeneration);
        if (!purge) return failTerminalLocked(purge.error());

        m_request = request;
        m_transition = purge.value();
        m_inFlightTransitionSequence =
            purge.value().transitionSequence;
        m_beganAt = beganAt.value();
        m_phase = MediaAvReacquisitionPhase::Purging;
        purgeWork = purge.value();
    }

    std::vector<::media::Result<MediaAvGenerationAcknowledgement>>
        purgeResults;
    purgeResults.reserve(m_participants.size());
    for (auto& participant : m_participants) {
        purgeResults.push_back(participant.purgeAll(purgeWork));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_phase != MediaAvReacquisitionPhase::Purging ||
        !m_transition ||
        !m_inFlightTransitionSequence ||
        *m_inFlightTransitionSequence != purgeWork.transitionSequence ||
        m_transition->transitionSequence != purgeWork.transitionSequence) {
        return failTerminalLocked(::media::ErrorInfo::internalError(
            "A/V reacquisition lost its in-flight purge transaction"));
    }

    for (const auto& purgeResult : purgeResults) {
        if (!purgeResult) {
            return failTerminalLocked(purgeResult.error());
        }
    }

    bool complete = false;
    for (auto& purgeResult : purgeResults) {
        auto acknowledged = m_transitionService->acknowledge(
            std::move(purgeResult).value());
        if (!acknowledged) {
            return failTerminalLocked(acknowledged.error());
        }
        complete = acknowledged.value();
    }
    if (complete) {
        m_phase = MediaAvReacquisitionPhase::Acquiring;
        m_beganAt.reset();
        m_inFlightTransitionSequence.reset();
    }
    return ::media::Status::success();
}

::media::Status MediaAvReacquisitionCoordinator::pollTimeout()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_phase != MediaAvReacquisitionPhase::Purging ||
        !m_beganAt) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "A/V reacquisition has no incomplete purge to poll"));
    }
    auto now = m_clock->now();
    if (!now) return failTerminalLocked(now.error());
    auto elapsed = now.value().checkedSubtract(*m_beganAt);
    if (!elapsed) return failTerminalLocked(elapsed.error());
    auto status =
        m_transitionService->pollTransitionTimeout(elapsed.value());
    return status ? status : failTerminalLocked(status.error());
}

MediaAvReacquisitionSnapshot
MediaAvReacquisitionCoordinator::snapshot() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return MediaAvReacquisitionSnapshot{
        m_phase,
        m_transition,
        m_request
            ? std::optional<MediaAvReacquisitionReason>(m_request->reason)
            : std::nullopt};
}

::media::Status
MediaAvReacquisitionCoordinator::markReadyForActivation(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_phase != MediaAvReacquisitionPhase::Acquiring ||
        !matchesTransition(generation, transitionSequence)) {
        return failTerminalLocked(::media::ErrorInfo::invalidArgument(
            "A/V reacquisition readiness requires the acquiring transition"));
    }
    m_phase = MediaAvReacquisitionPhase::ReadyForActivation;
    return ::media::Status::success();
}

::media::Status MediaAvReacquisitionCoordinator::markActivated(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    const auto active = m_transitionService->snapshot();
    if (m_phase != MediaAvReacquisitionPhase::ReadyForActivation ||
        !matchesTransition(generation, transitionSequence) ||
        active.poisoned ||
        active.readiness != MediaAvGenerationReadiness::Locked ||
        !active.playbackEpoch ||
        active.playbackEpoch->generation != generation ||
        !active.outputPermitted) {
        return failTerminalLocked(::media::ErrorInfo::invalidArgument(
            "A/V reacquisition activation requires the published matching epoch"));
    }
    m_phase = MediaAvReacquisitionPhase::Inactive;
    m_request.reset();
    m_transition.reset();
    m_inFlightTransitionSequence.reset();
    m_beganAt.reset();
    return ::media::Status::success();
}

void MediaAvReacquisitionCoordinator::abort() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_firstError) {
        m_firstError = ::media::ErrorInfo::cancelled(
            "A/V reacquisition coordinator was aborted");
    }
    m_transitionService->abort();
    m_phase = MediaAvReacquisitionPhase::Aborted;
    m_inFlightTransitionSequence.reset();
}

} // namespace media::ffmpeg::graph
