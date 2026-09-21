#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaAvGenerationEvidenceDisposition>
classifyMediaAvGenerationEvidence(
    const MediaAvReacquisitionSnapshot& reacquisition,
    const MediaAvEpochTransitionSnapshot& epoch,
    std::uint64_t generation)
{
    using Result = ::media::Result<MediaAvGenerationEvidenceDisposition>;
    if (generation == 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Generation evidence must have a nonzero authority"));
    if (reacquisition.phase == MediaAvReacquisitionPhase::Inactive) {
        if (reacquisition.transition || epoch.poisoned || !epoch.outputPermitted ||
            epoch.readiness != MediaAvGenerationReadiness::Locked ||
            !epoch.playbackEpoch || !epoch.audioOrigin ||
            epoch.audioOrigin->generation != epoch.playbackEpoch->generation)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Generation evidence requires a consistent published epoch"));
        if (generation < epoch.playbackEpoch->generation)
            return Result::success(MediaAvGenerationEvidenceDisposition::Retired);
        if (generation == epoch.playbackEpoch->generation)
            return Result::success(MediaAvGenerationEvidenceDisposition::Target);
        return Result::success(MediaAvGenerationEvidenceDisposition::Future);
    }
    const bool purging = reacquisition.phase == MediaAvReacquisitionPhase::Purging;
    if ((!purging && reacquisition.phase != MediaAvReacquisitionPhase::Acquiring &&
         reacquisition.phase != MediaAvReacquisitionPhase::ReadyForActivation) ||
        !reacquisition.transition || generation == 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Generation evidence requires a complete live transition"));
    const auto& transition = *reacquisition.transition;
    if (transition.publishedGeneration == 0 ||
        transition.oldGeneration < transition.publishedGeneration ||
        transition.nextGeneration <= transition.oldGeneration ||
        epoch.poisoned || epoch.outputPermitted ||
        epoch.readiness != (purging ? MediaAvGenerationReadiness::Reacquire
                                    : MediaAvGenerationReadiness::Acquiring) ||
        !epoch.playbackEpoch || !epoch.audioOrigin ||
        epoch.playbackEpoch->generation != transition.publishedGeneration ||
        epoch.audioOrigin->generation != transition.publishedGeneration)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Generation evidence requires a consistent closed transition epoch"));
    if (generation >= transition.publishedGeneration && generation <= transition.oldGeneration)
        return Result::success(MediaAvGenerationEvidenceDisposition::Retired);
    if (generation == transition.nextGeneration)
        return Result::success(MediaAvGenerationEvidenceDisposition::Target);
    if (generation > transition.nextGeneration)
        return Result::success(MediaAvGenerationEvidenceDisposition::Future);
    return Result::failure(::media::ErrorInfo::invalidArgument(
        "Generation evidence is outside the exact retired interval and planned target"));
}

MediaAvGenerationPublicationReservation::
    MediaAvGenerationPublicationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        std::unique_lock<std::mutex> activationLock) noexcept
    : m_owner(std::move(owner))
    , m_activationLock(std::move(activationLock))
{
}

MediaAvGenerationArbitrationReservation::
    MediaAvGenerationArbitrationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        MediaAvReacquisitionSnapshot reacquisition,
        MediaAvEpochTransitionSnapshot epoch,
        std::unique_lock<std::mutex> activationLock) noexcept
    : m_owner(std::move(owner))
    , m_reacquisition(std::move(reacquisition))
    , m_epoch(std::move(epoch))
    , m_activationLock(std::move(activationLock))
{
}

MediaAvGenerationPublicationReservation
MediaAvGenerationArbitrationReservation::
    retainPublicationAuthority() && noexcept
{
    return MediaAvGenerationPublicationReservation(
        std::move(m_owner), std::move(m_activationLock));
}

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
    MediaAvSyncGroupKey groupKey,
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants,
    std::vector<std::shared_ptr<MediaNodeWakeup>> domainWakeups,
    MediaAvSourceLifecyclePlan lifecycle,
    std::weak_ptr<const MediaAvSyncGroupRuntime> output)
    : m_groupKey(std::move(groupKey))
    , m_lifecycle(lifecycle)
    , m_output(std::move(output))
    , m_transitionService(std::move(transition))
    , m_clock(std::move(clock))
    , m_participants(std::move(participants))
    , m_domainWakeups(std::move(domainWakeups))
{
    auto wakeups = std::make_shared<const std::vector<std::shared_ptr<MediaNodeWakeup>>>(m_domainWakeups);
    for (auto& participant : m_participants) participant.bindPurgeProgressWakeups(wakeups);
}

::media::Result<std::shared_ptr<MediaAvReacquisitionCoordinator>>
MediaAvReacquisitionCoordinator::create(
    MediaAvSyncGroupKey groupKey,
    std::shared_ptr<MediaAvEpochTransitionService> transition,
    std::shared_ptr<MediaMasterClock> clock,
    std::vector<MediaAvGenerationParticipantGroup> participants,
    std::vector<std::shared_ptr<MediaNodeWakeup>> domainWakeups,
    MediaAvSourceLifecyclePlan lifecycle,
    std::weak_ptr<const MediaAvSyncGroupRuntime> output)
{
    if ((lifecycle.mode == MediaAvSourceLifecycleMode::PreserveActivatedOutput && output.expired()) ||
        (lifecycle.mode == MediaAvSourceLifecycleMode::FailSessionOnSourceLoss && !output.expired()) ||
        !groupKey.valid() || !transition || !transition->transitionPlan() || !clock || participants.empty() || domainWakeups.empty() ||
        std::any_of(domainWakeups.begin(), domainWakeups.end(),
                    [](const auto& wakeup) { return !wakeup; })) {
        return ::media::Result<
            std::shared_ptr<MediaAvReacquisitionCoordinator>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V reacquisition coordinator requires transition, clock, participants, and domain wakeups"));
    }
    return ::media::Result<
        std::shared_ptr<MediaAvReacquisitionCoordinator>>::success(
        std::shared_ptr<MediaAvReacquisitionCoordinator>(
            new MediaAvReacquisitionCoordinator(
                std::move(groupKey),
                std::move(transition),
                std::move(clock),
                std::move(participants), std::move(domainWakeups), lifecycle, std::move(output))));
}

::media::Status MediaAvReacquisitionCoordinator::observeClockEvidence(
    std::uint64_t generation, std::uint64_t revision)
{
    std::lock_guard lock(m_mutex);
    if (generation == 0 || revision == 0 ||
        (m_clockEvidence && (generation < m_clockEvidence->generation ||
                            revision < m_clockEvidence->revision)))
        return failTerminalLocked(::media::ErrorInfo::invalidArgument(
            "Source clock evidence must carry a monotonic generation and revision"));
    if (m_clockEvidence && revision == m_clockEvidence->revision) {
        if (generation != m_clockEvidence->generation)
            return failTerminalLocked(::media::ErrorInfo::invalidArgument(
                "Source clock generation changed without new evidence"));
        return ::media::Status::success();
    }
    auto now = m_clock->now();
    if (!now) return failTerminalLocked(now.error());
    m_clockEvidence = MediaAvSourceClockEvidence{generation, revision, now.value()};
    return ::media::Status::success();
}

std::optional<MediaAvSourceClockEvidence>
MediaAvReacquisitionCoordinator::clockEvidence() const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_clockEvidence;
}

bool MediaAvReacquisitionCoordinator::preservesActivatedOutput() const noexcept
{
    if (m_lifecycle.mode != MediaAvSourceLifecycleMode::PreserveActivatedOutput)
        return false;
    const auto output = m_output.lock();
    if (!output) return false;
    const auto active = output->epochTransitionSnapshot();
    return !active.poisoned && active.outputPermitted && active.playbackEpoch &&
        active.readiness == MediaAvGenerationReadiness::Locked;
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

::media::Result<bool> MediaAvReacquisitionCoordinator::requestSatisfiedLocked(
    const MediaAvReacquisitionRequest& request) const
{
    if (!preservesActivatedOutput() ||
        (request.reason != MediaAvReacquisitionReason::FutureGeneration &&
         request.reason != MediaAvReacquisitionReason::HardDiscontinuity))
        return ::media::Result<bool>::success(false);
    auto classified = classifyMediaAvGenerationEvidence(
        {m_phase, m_transition, m_request ? std::optional(m_request->reason) : std::nullopt},
        m_transitionService->snapshot(), request.observedGeneration);
    if (!classified) return ::media::Result<bool>::failure(classified.error());
    return ::media::Result<bool>::success(
        classified.value() == MediaAvGenerationEvidenceDisposition::Retired ||
        (classified.value() == MediaAvGenerationEvidenceDisposition::Target &&
         request.reason == MediaAvReacquisitionReason::FutureGeneration));
}

::media::Status MediaAvReacquisitionCoordinator::observe(
    MediaAvReacquisitionRequest request)
{
    auto activationLock = acquireActivationArbitration();
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    auto satisfied = requestSatisfiedLocked(request);
    if (!satisfied) return failTerminalLocked(satisfied.error());
    if (satisfied.value()) return ::media::Status::success();
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
    if (preservesActivatedOutput() && m_transition &&
        request.reason == MediaAvReacquisitionReason::FutureGeneration &&
        request.observedGeneration > m_transition->nextGeneration) {
        lock.unlock();
        activationLock.unlock();
        return this->request(request);
    }
    return failTerminalLocked(
        ::media::ErrorInfo::invalidArgument(
            "A/V reacquisition rejects incompatible generation evidence after transition begin"));
}

::media::Status MediaAvReacquisitionCoordinator::beginRequestLocked(
    MediaAvReacquisitionRequest request)
{
    if (m_firstError) return ::media::Status::failure(*m_firstError);
    auto satisfied = requestSatisfiedLocked(request);
    if (!satisfied) return failTerminalLocked(satisfied.error());
    if (satisfied.value()) return ::media::Status::success();
    if (m_phase != MediaAvReacquisitionPhase::Inactive && matchesActiveRequest(request))
        return ::media::Status::success();
    const auto active = m_transitionService->snapshot();
    MediaAvTransitionOrigin origin = MediaAvPublishedGeneration{0};
    std::uint64_t oldGeneration = 0;
    if (m_phase == MediaAvReacquisitionPhase::Inactive) {
        auto queued = validateAndQueueRequest(request);
        if (!queued) return failTerminalLocked(queued.error());
        request = *m_request;
        if (active.poisoned || active.readiness != MediaAvGenerationReadiness::Locked ||
            !active.playbackEpoch) {
            return failTerminalLocked(::media::ErrorInfo::notInitialized(
                "A/V reacquisition lost its active locked playback epoch"));
        }
        oldGeneration = active.playbackEpoch->generation;
        origin = MediaAvPublishedGeneration{oldGeneration};
    } else {
        const bool future = request.reason == MediaAvReacquisitionReason::FutureGeneration;
        if (!preservesActivatedOutput() || !m_transition || active.poisoned ||
            active.outputPermitted || !active.playbackEpoch ||
            (future ? request.observedGeneration <= m_transition->nextGeneration
                    : request.observedGeneration != m_transition->nextGeneration)) {
            return failTerminalLocked(::media::ErrorInfo::invalidArgument(
                "A/V reacquisition rejects incompatible unpublished generation evidence"));
        }
        if (m_phase == MediaAvReacquisitionPhase::Purging) {
            if (!m_queuedRequest || request.observedGeneration > m_queuedRequest->observedGeneration)
                m_queuedRequest = request;
            return ::media::Status::success();
        }
        if ((m_phase != MediaAvReacquisitionPhase::Acquiring &&
             m_phase != MediaAvReacquisitionPhase::ReadyForActivation) ||
            m_inFlightTransitionSequence ||
            active.readiness != MediaAvGenerationReadiness::Acquiring) {
            return failTerminalLocked(::media::ErrorInfo::invalidArgument(
                "A/V unpublished retirement requires completed purge and closed publication"));
        }
        oldGeneration = m_transition->nextGeneration;
        origin = MediaAvUnpublishedAcquisition{
            oldGeneration, m_transition->transitionSequence};
    }
    const bool future = request.reason == MediaAvReacquisitionReason::FutureGeneration;
    if (!future && oldGeneration == std::numeric_limits<std::uint64_t>::max())
        return failTerminalLocked(::media::ErrorInfo::invalidArgument(
            "A/V source generation exhausted"));
    const std::uint64_t nextGeneration = future ? request.observedGeneration : oldGeneration + 1;
    auto beganAt = m_clock->now();
    if (!beganAt) return failTerminalLocked(beganAt.error());
    auto purge = m_transitionService->beginReacquisition(std::move(origin), nextGeneration);
    if (!purge) return failTerminalLocked(purge.error());
    m_request = request;
    m_transition = purge.value();
    m_inFlightTransitionSequence = purge.value().transitionSequence;
    m_beganAt = beganAt.value();
    m_phase = MediaAvReacquisitionPhase::Purging;
    return ::media::Status::success();
}

::media::Status MediaAvReacquisitionCoordinator::request(
    MediaAvReacquisitionRequest request)
{
    std::optional<MediaAvGenerationPurge> began;
    {
        auto publicationLock = acquireActivationArbitration();
        std::lock_guard stateLock(m_mutex);
        const auto previousSequence = m_transition
            ? std::optional<std::uint64_t>(m_transition->transitionSequence) : std::nullopt;
        auto status = beginRequestLocked(request);
        if (!status) return status;
        if (m_phase == MediaAvReacquisitionPhase::Purging && m_transition &&
            previousSequence != m_transition->transitionSequence)
            began = m_transition;
    }
    if (began) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "av_reacquisition group=" + m_groupKey.value() + " phase=purging old=" +
                std::to_string(began->oldGeneration) + " next=" +
                std::to_string(began->nextGeneration) + " transition=" +
                std::to_string(began->transitionSequence));
        for (const auto& wakeup : m_domainWakeups) wakeup->notify();
    }
    return ::media::Status::success();
}

::media::Result<std::optional<MediaRunningTime>>
MediaAvReacquisitionCoordinator::progressPurge()
{
    using Result = ::media::Result<std::optional<MediaRunningTime>>;
    // Child callbacks can acquire publication authority; never hold either
    // coordinator state or activation arbitration while calling them.
    std::unique_lock progressLock(m_purgeMutex);
    MediaAvGenerationPurge purgeWork{};
    MediaRunningTime beganAt = MediaRunningTime::fromNanoseconds(0);
    {
        std::lock_guard lock(m_mutex);
        if (m_firstError) return Result::failure(*m_firstError);
        if (m_phase != MediaAvReacquisitionPhase::Purging)
            return Result::success(std::nullopt);
        if (!m_transition || !m_beganAt) return Result::failure(
            failTerminalLocked(::media::ErrorInfo::internalError(
                "A/V purge progress lost its transaction")).error());
        purgeWork = *m_transition;
        beganAt = *m_beganAt;
    }
    std::vector<MediaAvGenerationAcknowledgement> acknowledgements;
    std::optional<::media::ErrorInfo> failure;
    for (auto& participant : m_participants) {
        auto result = participant.purgeAll(purgeWork);
        if (!result) {
            if (!failure) failure = result.error();
        } else if (result.value()) {
            acknowledgements.push_back(std::move(*result.value()));
        }
    }
    auto activationLock = acquireActivationArbitration();
    std::unique_lock lock(m_mutex);
    if (m_firstError) return Result::failure(*m_firstError);
    if (failure) return Result::failure(failTerminalLocked(*failure).error());
    if (m_phase != MediaAvReacquisitionPhase::Purging || !m_transition ||
        m_transition->transitionSequence != purgeWork.transitionSequence)
        return Result::failure(failTerminalLocked(::media::ErrorInfo::internalError(
            "A/V purge progress lost its in-flight transaction")).error());
    auto now = m_clock->now();
    auto deadline = beganAt.checkedAdd(m_transitionService->transitionPlan()->acknowledgementTimeout);
    if (!now || !deadline) return Result::failure(
        failTerminalLocked(!now ? now.error() : deadline.error()).error());
    auto elapsed = now.value().checkedSubtract(beganAt);
    if (!elapsed) return Result::failure(failTerminalLocked(elapsed.error()).error());
    auto timeout = m_transitionService->pollTransitionTimeout(elapsed.value());
    if (!timeout) return Result::failure(failTerminalLocked(timeout.error()).error());
    bool complete = false;
    for (auto& acknowledgement : acknowledgements) {
        auto acknowledged = m_transitionService->acknowledge(std::move(acknowledgement));
        if (!acknowledged) return Result::failure(failTerminalLocked(acknowledged.error()).error());
        complete = acknowledged.value();
    }
    if (!complete) return Result::success(deadline.value());
    m_phase = MediaAvReacquisitionPhase::Acquiring;
    m_beganAt.reset();
    m_inFlightTransitionSequence.reset();
    const auto queued = std::exchange(m_queuedRequest, std::nullopt);
    std::optional<MediaRunningTime> nextDeadline;
    std::optional<MediaAvGenerationPurge> continuedPurge;
    if (queued) {
        // Ack completion and replacement share activation arbitration. No
        // observer can publish the retired pending generation between them.
        auto requested = beginRequestLocked(*queued);
        if (!requested) return Result::failure(requested.error());
        if (m_firstError) return Result::failure(*m_firstError);
        if (m_phase == MediaAvReacquisitionPhase::Purging) {
            if (!m_beganAt || !m_transition || !m_inFlightTransitionSequence ||
                *m_inFlightTransitionSequence != m_transition->transitionSequence)
                return Result::failure(failTerminalLocked(::media::ErrorInfo::internalError(
                    "Queued source transition lost its stable deadline transaction")).error());
            auto deadlineForNext = m_beganAt->checkedAdd(
                m_transitionService->transitionPlan()->acknowledgementTimeout);
            if (!deadlineForNext)
                return Result::failure(failTerminalLocked(deadlineForNext.error()).error());
            nextDeadline = deadlineForNext.value();
            continuedPurge = m_transition;
        } else if (m_phase != MediaAvReacquisitionPhase::Acquiring ||
                   m_beganAt || m_inFlightTransitionSequence) {
            return Result::failure(failTerminalLocked(::media::ErrorInfo::internalError(
                "Queued source transition produced an inconsistent completion phase")).error());
        }
    }
    const auto completedPhase = m_phase;
    lock.unlock();
    activationLock.unlock();
    progressLock.unlock();
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "av_reacquisition group=" + m_groupKey.value() +
        " event=purge_completed purge_ack=complete continuation=" +
        std::string(completedPhase == MediaAvReacquisitionPhase::Purging ? "purging" : "acquiring") +
        " ack_count=" +
        std::to_string(m_participants.size()) + " old=" +
        std::to_string(purgeWork.oldGeneration) + " next=" +
        std::to_string(purgeWork.nextGeneration) + " transition=" +
        std::to_string(purgeWork.transitionSequence));
    if (continuedPurge) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "av_reacquisition group=" + m_groupKey.value() +
                " event=queued_begin phase=purging old=" +
                std::to_string(continuedPurge->oldGeneration) + " next=" +
                std::to_string(continuedPurge->nextGeneration) + " transition=" +
                std::to_string(continuedPurge->transitionSequence) + " published_generation=" +
                std::to_string(continuedPurge->publishedGeneration));
    }
    for (const auto& wakeup : m_domainWakeups) wakeup->notify();
    return Result::success(nextDeadline);
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

MediaAvGenerationArbitrationReservation
MediaAvReacquisitionCoordinator::reserveGenerationArbitration()
{
    auto activationLock = acquireActivationArbitration();
    std::lock_guard<std::mutex> lock(m_mutex);
    MediaAvReacquisitionSnapshot reacquisition{
        m_phase,
        m_transition,
        m_request
            ? std::optional<MediaAvReacquisitionReason>(m_request->reason)
            : std::nullopt};
    return MediaAvGenerationArbitrationReservation(
        shared_from_this(),
        std::move(reacquisition),
        m_transitionService->snapshot(),
        std::move(activationLock));
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
