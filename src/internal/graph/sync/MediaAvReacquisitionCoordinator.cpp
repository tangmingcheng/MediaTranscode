#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"

#include <exception>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaAvStartupReleasePublicationReservation::
    MediaAvStartupReleasePublicationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        MediaAvStartupReleaseDisposition disposition,
        std::unique_lock<std::mutex> publicationLock) noexcept
    : m_owner(std::move(owner))
    , m_disposition(disposition)
    , m_publicationLock(std::move(publicationLock))
{
}

MediaAvStartupReleasePublicationReservation::
    MediaAvStartupReleasePublicationReservation(
        MediaAvStartupReleasePublicationReservation&& other) noexcept
    : m_owner(std::move(other.m_owner))
    , m_disposition(other.m_disposition)
    , m_publicationLock(std::move(other.m_publicationLock))
{
    other.m_disposition = MediaAvStartupReleaseDisposition::Reject;
}

MediaAvStartupReleasePublicationReservation&
MediaAvStartupReleasePublicationReservation::operator=(
    MediaAvStartupReleasePublicationReservation&& other) noexcept
{
    if (this == &other) return *this;
    if (m_publicationLock.owns_lock()) m_publicationLock.unlock();
    m_owner.reset();
    m_disposition = other.m_disposition;
    m_publicationLock = std::move(other.m_publicationLock);
    m_owner = std::move(other.m_owner);
    other.m_disposition = MediaAvStartupReleaseDisposition::Reject;
    return *this;
}

MediaAvStartupReleaseDisposition
MediaAvStartupReleasePublicationReservation::disposition() const noexcept
{
    return m_disposition;
}

void MediaAvStartupReleasePublicationReservation::completePublished() noexcept
{
    if (m_publicationLock.owns_lock()) m_publicationLock.unlock();
    m_owner.reset();
}

MediaAvReacquisitionActivationReservation::
    MediaAvReacquisitionActivationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        std::uint64_t generation,
        std::uint64_t transitionSequence,
        std::unique_lock<std::mutex> activationLock) noexcept
    : m_owner(std::move(owner))
    , m_generation(generation)
    , m_transitionSequence(transitionSequence)
    , m_activationLock(std::move(activationLock))
{
}

MediaAvReacquisitionActivationReservation::
    MediaAvReacquisitionActivationReservation(
        MediaAvReacquisitionActivationReservation&& other) noexcept
    : m_owner(std::move(other.m_owner))
    , m_generation(other.m_generation)
    , m_transitionSequence(other.m_transitionSequence)
    , m_activationLock(std::move(other.m_activationLock))
    , m_authorized(other.m_authorized)
    , m_finalized(other.m_finalized)
    , m_completed(other.m_completed)
{
}

MediaAvReacquisitionActivationReservation&
MediaAvReacquisitionActivationReservation::operator=(
    MediaAvReacquisitionActivationReservation&& other) noexcept
{
    if (this == &other) return *this;
    abandon();
    m_owner = std::move(other.m_owner);
    m_generation = other.m_generation;
    m_transitionSequence = other.m_transitionSequence;
    m_activationLock = std::move(other.m_activationLock);
    m_authorized = other.m_authorized;
    m_finalized = other.m_finalized;
    m_completed = other.m_completed;
    return *this;
}

MediaAvReacquisitionActivationReservation::
    ~MediaAvReacquisitionActivationReservation()
{
    abandon();
}

::media::Status
MediaAvReacquisitionActivationReservation::authorizePublication()
{
    return m_owner
        ? m_owner->authorizePublication(*this)
        : ::media::Status::failure(::media::ErrorInfo::cancelled(
              "A/V reacquisition activation reservation is inactive"));
}

::media::Status
MediaAvReacquisitionActivationReservation::finalizePublication()
{
    return m_owner
        ? m_owner->finalizePublication(*this)
        : ::media::Status::failure(::media::ErrorInfo::cancelled(
              "A/V reacquisition activation reservation is inactive"));
}

void MediaAvReacquisitionActivationReservation::completePublished() noexcept
{
    if (m_owner && m_finalized) {
        m_owner->releasePublished(*this);
    } else {
        abandon();
    }
}

void MediaAvReacquisitionActivationReservation::abandon() noexcept
{
    if (m_owner) m_owner->abandon(*this);
}

MediaAvReacquisitionCoordinator::MediaAvReacquisitionCoordinator(
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants)
    : m_transitionService(std::move(transition))
    , m_clock(std::move(clock))
    , m_participants(std::move(participants))
{
}

::media::Result<std::shared_ptr<MediaAvReacquisitionCoordinator>>
MediaAvReacquisitionCoordinator::create(
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants)
{
    if (!transition || !clock || participants.empty()) {
        return ::media::Result<
            std::shared_ptr<MediaAvReacquisitionCoordinator>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V reacquisition coordinator requires transition, clock, and participants"));
    }
    return ::media::Result<
        std::shared_ptr<MediaAvReacquisitionCoordinator>>::success(
        std::shared_ptr<MediaAvReacquisitionCoordinator>(
            new MediaAvReacquisitionCoordinator(
                std::move(transition),
                std::move(clock),
                std::move(participants))));
}

std::unique_lock<std::mutex>
MediaAvReacquisitionCoordinator::acquireActivationArbitration()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_activationWaiters;
        m_activationWaitChanged.notify_all();
    }
    std::unique_lock<std::mutex> activationLock(m_activationMutex);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        --m_activationWaiters;
        m_activationWaitChanged.notify_all();
    }
    return activationLock;
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
    auto activationLock = acquireActivationArbitration();
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
    return failTerminalLocked(
        ::media::ErrorInfo::invalidArgument(
            "A/V reacquisition rejects incompatible generation evidence after transition begin"));
}

::media::Status MediaAvReacquisitionCoordinator::request(
    MediaAvReacquisitionRequest request)
{
    MediaAvGenerationPurge purgeWork{};
    {
        std::unique_lock<std::mutex> stateLock(m_mutex);
        if (m_firstError) {
            return ::media::Status::failure(*m_firstError);
        }
        if (m_phase != MediaAvReacquisitionPhase::Inactive) {
            if (matchesActiveRequest(request)) {
                return ::media::Status::success();
            }
            stateLock.unlock();
            return rejectIncompatibleEvidence(
                ::media::ErrorInfo::invalidArgument(
                    "A/V reacquisition rejects an incompatible request after transition begin"));
        }
    }

    {
        auto publicationLock = acquireActivationArbitration();
        std::lock_guard<std::mutex> stateLock(m_mutex);
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
    auto activationLock = acquireActivationArbitration();
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

MediaAvStartupReleaseDisposition
MediaAvReacquisitionCoordinator::classifyRelease(
    MediaAvStartupReleaseKind kind,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return classifyReleaseLocked(kind, generation, transitionSequence);
}

MediaAvStartupReleaseDisposition
MediaAvReacquisitionCoordinator::classifyReleaseLocked(
    MediaAvStartupReleaseKind kind,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence) const noexcept
{
    if (m_firstError || m_phase == MediaAvReacquisitionPhase::Aborted) {
        return MediaAvStartupReleaseDisposition::Reject;
    }
    if (m_phase != MediaAvReacquisitionPhase::Inactive) {
        if (!m_transition) {
            return MediaAvStartupReleaseDisposition::Reject;
        }
        if (generation <= m_transition->oldGeneration) {
            return MediaAvStartupReleaseDisposition::DropOld;
        }
        if (generation == m_transition->nextGeneration) {
            return MediaAvStartupReleaseDisposition::Withhold;
        }
        return MediaAvStartupReleaseDisposition::Reject;
    }

    const auto active = m_transitionService->snapshot();
    if (active.poisoned ||
        active.readiness != MediaAvGenerationReadiness::Locked ||
        !active.playbackEpoch ||
        !active.outputPermitted) {
        return MediaAvStartupReleaseDisposition::Reject;
    }
    if (generation < active.playbackEpoch->generation) {
        return MediaAvStartupReleaseDisposition::DropOld;
    }
    if (generation != active.playbackEpoch->generation) {
        return MediaAvStartupReleaseDisposition::Reject;
    }
    if (kind == MediaAvStartupReleaseKind::NextAtomicRelease &&
        (!transitionSequence ||
         transitionSequence != m_lastPublishedTransitionSequence)) {
        return MediaAvStartupReleaseDisposition::Reject;
    }
    if (kind != MediaAvStartupReleaseKind::NextAtomicRelease &&
        transitionSequence) {
        return MediaAvStartupReleaseDisposition::Reject;
    }
    return MediaAvStartupReleaseDisposition::Publish;
}

MediaAvStartupReleasePublicationReservation
MediaAvReacquisitionCoordinator::reserveReleasePublication(
    MediaAvStartupReleaseKind kind,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence)
{
    auto publicationLock = acquireActivationArbitration();
    MediaAvStartupReleaseDisposition disposition;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        disposition =
            classifyReleaseLocked(kind, generation, transitionSequence);
    }
    if (disposition != MediaAvStartupReleaseDisposition::Publish) {
        publicationLock.unlock();
    }
    return MediaAvStartupReleasePublicationReservation(
        shared_from_this(), disposition, std::move(publicationLock));
}

::media::Status
MediaAvReacquisitionCoordinator::markReadyForActivation(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    auto activationLock = acquireActivationArbitration();
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

::media::Status
MediaAvReacquisitionCoordinator::rejectIncompatibleEvidence(
    ::media::ErrorInfo error)
{
    auto activationLock = acquireActivationArbitration();
    std::lock_guard<std::mutex> stateLock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    return failTerminalLocked(std::move(error));
}

::media::Result<MediaAvReacquisitionActivationReservation>
MediaAvReacquisitionCoordinator::reserveActivation(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    auto activationLock = acquireActivationArbitration();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Result<
            MediaAvReacquisitionActivationReservation>::failure(
            *m_firstError);
    }
    if (m_phase != MediaAvReacquisitionPhase::ReadyForActivation ||
        !matchesTransition(generation, transitionSequence)) {
        auto failed = failTerminalLocked(
            ::media::ErrorInfo::invalidArgument(
                "A/V reacquisition activation reservation requires the ready transition"));
        return ::media::Result<
            MediaAvReacquisitionActivationReservation>::failure(
            failed.error());
    }
    m_phase = MediaAvReacquisitionPhase::Activating;
    return ::media::Result<
        MediaAvReacquisitionActivationReservation>::success(
        MediaAvReacquisitionActivationReservation(
            shared_from_this(),
            generation,
            transitionSequence,
            std::move(activationLock)));
}

::media::Status MediaAvReacquisitionCoordinator::authorizePublication(
    MediaAvReacquisitionActivationReservation& reservation)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    const auto active = m_transitionService->snapshot();
    if (reservation.m_owner.get() != this ||
        reservation.m_authorized ||
        reservation.m_completed ||
        m_phase != MediaAvReacquisitionPhase::Activating ||
        !matchesTransition(
            reservation.m_generation,
            reservation.m_transitionSequence) ||
        active.poisoned ||
        active.readiness != MediaAvGenerationReadiness::Locked ||
        !active.playbackEpoch ||
        active.playbackEpoch->generation != reservation.m_generation ||
        !active.outputPermitted) {
        return failTerminalLocked(::media::ErrorInfo::invalidArgument(
            "A/V reacquisition publication authorization requires the activated reserved epoch"));
    }
    m_phase = MediaAvReacquisitionPhase::Publishing;
    reservation.m_authorized = true;
    return ::media::Status::success();
}

::media::Status MediaAvReacquisitionCoordinator::finalizePublication(
    MediaAvReacquisitionActivationReservation& reservation)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (reservation.m_owner.get() != this ||
            !reservation.m_authorized ||
            reservation.m_finalized ||
            reservation.m_completed ||
            m_phase != MediaAvReacquisitionPhase::Publishing ||
            !matchesTransition(
                reservation.m_generation,
                reservation.m_transitionSequence)) {
            return failTerminalLocked(::media::ErrorInfo::invalidArgument(
                "A/V reacquisition finalization requires the publishing reserved epoch"));
        }
        m_phase = MediaAvReacquisitionPhase::Inactive;
        m_lastPublishedTransitionSequence =
            reservation.m_transitionSequence;
        m_request.reset();
        m_transition.reset();
        m_inFlightTransitionSequence.reset();
        m_beganAt.reset();
        reservation.m_finalized = true;
    }
    return ::media::Status::success();
}

void MediaAvReacquisitionCoordinator::releasePublished(
    MediaAvReacquisitionActivationReservation& reservation) noexcept
{
    reservation.m_completed = true;
    if (reservation.m_activationLock.owns_lock()) {
        reservation.m_activationLock.unlock();
    }
    reservation.m_owner.reset();
}

void MediaAvReacquisitionCoordinator::abandon(
    MediaAvReacquisitionActivationReservation& reservation) noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!reservation.m_completed && !m_firstError) {
            (void)failTerminalLocked(::media::ErrorInfo::cancelled(
                reservation.m_authorized
                    ? "A/V reacquisition publication reservation was abandoned"
                    : "A/V reacquisition activation reservation was abandoned"));
        }
    }
    if (reservation.m_activationLock.owns_lock()) {
        reservation.m_activationLock.unlock();
    }
    reservation.m_owner.reset();
}

void MediaAvReacquisitionCoordinator::abort() noexcept
{
    auto activationLock = acquireActivationArbitration();
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
