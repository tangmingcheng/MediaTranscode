#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <stdexcept>
#include <utility>

namespace media::ffmpeg::graph {

MediaProtocolOutputGenerationCommitReservation::
    MediaProtocolOutputGenerationCommitReservation(
        MediaAvOutputPermitCommitReservation outputPermit,
        std::unique_lock<std::mutex> stateLock,
        std::unique_lock<std::mutex> sessionLock) noexcept
    : m_outputPermit(std::move(outputPermit))
    , m_stateLock(std::move(stateLock))
    , m_sessionLock(std::move(sessionLock))
{
}

MediaProtocolOutputGenerationState::MediaProtocolOutputGenerationState(
    std::string plannedIdentity,
    std::shared_ptr<MediaProtocolOutputGenerationSessionState> sessionState)
    : m_plannedIdentity(std::move(plannedIdentity))
    , m_sessionState(std::move(sessionState))
{
    if (!m_sessionState) {
        throw std::invalid_argument(
            "Protocol output generation state requires a session state");
    }
}

std::string_view
MediaProtocolOutputGenerationState::plannedIdentity() const noexcept
{
    return m_plannedIdentity;
}

const std::shared_ptr<MediaProtocolOutputGenerationSessionState>&
MediaProtocolOutputGenerationState::sessionState() const noexcept
{
    return m_sessionState;
}

::media::Result<MediaProtocolOutputGenerationCommitReservation>
MediaProtocolOutputGenerationState::permitActivatedGeneration(
    const MediaAvSyncGroupRuntime& group,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence)
{
    auto outputPermit = group.reserveOutputCommit(generation);
    if (!outputPermit) {
        return ::media::Result<
            MediaProtocolOutputGenerationCommitReservation>::failure(
                outputPermit.error());
    }
    std::unique_lock stateLock(m_mutex);
    const bool initialActivation =
        !m_permittedGeneration && !m_pendingGeneration &&
        !m_pendingTransitionSequence && !m_lastTransitionSequence &&
        !transitionSequence;
    const bool plannedRollover =
        m_pendingGeneration && m_pendingTransitionSequence &&
        transitionSequence &&
        *m_pendingGeneration == generation &&
        *m_pendingTransitionSequence == *transitionSequence;
    if (m_plannedIdentity.empty() || generation == 0 ||
        (!initialActivation && !plannedRollover)) {
        return ::media::Result<
            MediaProtocolOutputGenerationCommitReservation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output activation requires the exact planned generation transition"));
    }
    m_permittedGeneration = generation;
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    std::unique_lock sessionLock(m_sessionState->m_mutex);
    return ::media::Result<
        MediaProtocolOutputGenerationCommitReservation>::success(
            MediaProtocolOutputGenerationCommitReservation(
                std::move(outputPermit).value(), std::move(stateLock),
                std::move(sessionLock)));
}

::media::Result<MediaProtocolOutputGenerationCommitReservation>
MediaProtocolOutputGenerationState::reserveCommit(
    const MediaAvSyncGroupRuntime& group,
    std::uint64_t generation) const
{
    auto outputPermit = group.reserveOutputCommit(generation);
    if (!outputPermit) {
        return ::media::Result<
            MediaProtocolOutputGenerationCommitReservation>::failure(
                outputPermit.error());
    }
    std::unique_lock lock(m_mutex);
    if (m_plannedIdentity.empty() || generation == 0 ||
        !m_permittedGeneration || *m_permittedGeneration != generation) {
        return ::media::Result<
            MediaProtocolOutputGenerationCommitReservation>::failure(
                ::media::ErrorInfo::cancelled(
                    "Protocol output commit requires the exact permitted generation"));
    }
    std::unique_lock sessionLock(m_sessionState->m_mutex);
    return ::media::Result<
        MediaProtocolOutputGenerationCommitReservation>::success(
            MediaProtocolOutputGenerationCommitReservation(
                std::move(outputPermit).value(), std::move(lock),
                std::move(sessionLock)));
}

::media::Result<MediaProtocolOutputAuthorityActivation>
MediaProtocolOutputGenerationState::permitAuthorityActivation(
    const MediaAvSyncGroupRuntime& group)
{
    auto activated = group.reserveActivatedOutput();
    if (!activated) {
        return ::media::Result<
            MediaProtocolOutputAuthorityActivation>::failure(
                activated.error());
    }
    const auto epoch = activated.value().epoch;
    const auto transitionSequence =
        activated.value().completedTransitionSequence;
    std::unique_lock stateLock(m_mutex);
    const bool initialActivation =
        !m_permittedGeneration && !m_pendingGeneration &&
        !m_pendingTransitionSequence && !m_lastTransitionSequence &&
        !transitionSequence;
    const bool plannedRollover =
        m_pendingGeneration && m_pendingTransitionSequence &&
        transitionSequence &&
        *m_pendingGeneration == epoch.generation &&
        *m_pendingTransitionSequence == *transitionSequence;
    if (m_plannedIdentity.empty() || epoch.generation == 0 ||
        (!initialActivation && !plannedRollover)) {
        return ::media::Result<
            MediaProtocolOutputAuthorityActivation>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Protocol output authority activation requires the exact planned transition"));
    }
    m_permittedGeneration = epoch.generation;
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    std::unique_lock sessionLock(m_sessionState->m_mutex);
    auto permit = std::move(activated).value().reservation;
    return ::media::Result<
        MediaProtocolOutputAuthorityActivation>::success(
            MediaProtocolOutputAuthorityActivation{
                epoch,
                MediaProtocolOutputGenerationCommitReservation(
                    std::move(permit), std::move(stateLock),
                    std::move(sessionLock))});
}

MediaProtocolOutputGenerationSessionMutationReservation
MediaProtocolOutputGenerationState::reserveSessionMutation() const
{
    std::unique_lock stateLock(m_mutex);
    std::unique_lock sessionLock(m_sessionState->m_mutex);
    return MediaProtocolOutputGenerationSessionMutationReservation(
        std::move(stateLock), std::move(sessionLock));
}

::media::Status MediaProtocolOutputGenerationState::purge(
    const MediaAvGenerationPurge& purge)
{
    std::lock_guard lock(m_mutex);
    std::lock_guard sessionLock(m_sessionState->m_mutex);
    if (m_plannedIdentity.empty() || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        !m_permittedGeneration ||
        *m_permittedGeneration != purge.oldGeneration ||
        m_pendingGeneration || m_pendingTransitionSequence ||
        (m_lastTransitionSequence &&
         purge.transitionSequence <= *m_lastTransitionSequence)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output purge requires exact generations and a fresh transition"));
    }
    m_permittedGeneration.reset();
    m_pendingGeneration = purge.nextGeneration;
    m_pendingTransitionSequence = purge.transitionSequence;
    m_lastTransitionSequence = purge.transitionSequence;
    m_sessionState->resetForGenerationPurge();
    return ::media::Status::success();
}

void MediaProtocolOutputGenerationState::resetLifecycle() noexcept
{
    std::lock_guard lock(m_mutex);
    std::lock_guard sessionLock(m_sessionState->m_mutex);
    m_permittedGeneration.reset();
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    m_lastTransitionSequence.reset();
    m_sessionState->resetForGenerationPurge();
}

} // namespace media::ffmpeg::graph
