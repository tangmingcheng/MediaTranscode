#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <stdexcept>
#include <utility>

namespace media::ffmpeg::graph {

MediaProtocolOutputGenerationCommitReservation::
    MediaProtocolOutputGenerationCommitReservation(
        MediaProtocolOutputCommitReservation outputPermit,
        std::unique_lock<std::mutex> stateLock,
        std::unique_lock<std::mutex> sessionLock,
        std::optional<std::uint64_t> completedTransitionSequence) noexcept
    : m_outputPermit(std::move(outputPermit))
    , m_stateLock(std::move(stateLock))
    , m_sessionLock(std::move(sessionLock))
    , m_completedTransitionSequence(completedTransitionSequence)
{
}

bool MediaProtocolOutputGenerationCommitReservation::
    startsAfterGenerationTransition() const noexcept
{
    return m_completedTransitionSequence.has_value();
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
    const MediaProtocolOutputRuntimeAuthority& authority,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence)
{
    auto outputPermit = authority.reserveCommit(generation);
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
                std::move(sessionLock), transitionSequence));
}

::media::Result<MediaProtocolOutputGenerationCommitReservation>
MediaProtocolOutputGenerationState::reserveCommit(
    const MediaProtocolOutputRuntimeAuthority& authority,
    std::uint64_t generation) const
{
    auto outputPermit = authority.reserveCommit(generation);
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
                std::move(sessionLock), m_lastTransitionSequence));
}

::media::Result<MediaProtocolOutputGenerationState::GenerationDisposition>
MediaProtocolOutputGenerationState::classifyGeneration(
    std::uint64_t generation) const
{
    std::lock_guard lock(m_mutex);
    if (m_plannedIdentity.empty() || generation == 0) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output generation classification requires exact planned authority"));
    }
    if (!m_permittedGeneration && !m_pendingGeneration) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "Protocol output generation classification requires active planned generation authority"));
    }
    if (!m_permittedGeneration) {
        return ::media::Result<GenerationDisposition>::success(
            generation < *m_pendingGeneration
                ? GenerationDisposition::Old
                : GenerationDisposition::Future);
    }
    if (generation < *m_permittedGeneration) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::Old);
    }
    if (generation > *m_permittedGeneration) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::Future);
    }
    return ::media::Result<GenerationDisposition>::success(
        GenerationDisposition::Current);
}

::media::Result<MediaProtocolOutputAuthorityActivation>
MediaProtocolOutputGenerationState::permitAuthorityActivation(
    const MediaProtocolOutputRuntimeAuthority& authority)
{
    auto activated = authority.currentActivation();
    if (!activated) {
        return ::media::Result<
            MediaProtocolOutputAuthorityActivation>::failure(
                activated.error());
    }
    const auto activation = activated.value();
    const auto transitionSequence =
        activation.completedTransitionSequence;
    std::unique_lock stateLock(m_mutex);
    const bool initialActivation =
        !m_permittedGeneration && !m_pendingGeneration &&
        !m_pendingTransitionSequence && !m_lastTransitionSequence &&
        !transitionSequence;
    const bool plannedRollover =
        m_pendingGeneration && m_pendingTransitionSequence &&
        transitionSequence &&
        *m_pendingGeneration == activation.generation &&
        *m_pendingTransitionSequence == *transitionSequence;
    if (m_plannedIdentity.empty() || activation.generation == 0 ||
        (!initialActivation && !plannedRollover)) {
        return ::media::Result<
            MediaProtocolOutputAuthorityActivation>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Protocol output authority activation requires the exact planned transition"));
    }
    auto permit = authority.reserveCommit(activation.generation);
    if (!permit) {
        return ::media::Result<
            MediaProtocolOutputAuthorityActivation>::failure(
                permit.error());
    }
    m_permittedGeneration = activation.generation;
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    std::unique_lock sessionLock(m_sessionState->m_mutex);
    return ::media::Result<
        MediaProtocolOutputAuthorityActivation>::success(
            MediaProtocolOutputAuthorityActivation{
                activation,
                MediaProtocolOutputGenerationCommitReservation(
                    std::move(permit).value(), std::move(stateLock),
                    std::move(sessionLock), transitionSequence)});
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
    const bool purgePermittedGeneration =
        m_permittedGeneration &&
        *m_permittedGeneration == purge.oldGeneration &&
        !m_pendingGeneration && !m_pendingTransitionSequence;
    const bool supersedeUnpublishedGeneration =
        !m_permittedGeneration &&
        m_pendingGeneration &&
        *m_pendingGeneration == purge.oldGeneration &&
        m_pendingTransitionSequence &&
        m_lastTransitionSequence &&
        *m_pendingTransitionSequence == *m_lastTransitionSequence;
    if (m_plannedIdentity.empty() || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        (!purgePermittedGeneration &&
         !supersedeUnpublishedGeneration) ||
        (m_lastTransitionSequence &&
         purge.transitionSequence <= *m_lastTransitionSequence)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output purge requires exact generations and a fresh transition"));
    }
    auto prepared = m_sessionState->prepareForGenerationPurge();
    if (!prepared) return prepared;
    m_permittedGeneration.reset();
    m_pendingGeneration = purge.nextGeneration;
    m_pendingTransitionSequence = purge.transitionSequence;
    m_lastTransitionSequence = purge.transitionSequence;
    m_sessionState->resetForGenerationPurge();
    return ::media::Status::success();
}

::media::Status MediaProtocolOutputGenerationState::resetLifecycle()
{
    std::lock_guard lock(m_mutex);
    std::lock_guard sessionLock(m_sessionState->m_mutex);
    auto prepared = m_sessionState->prepareForGenerationPurge();
    if (!prepared) return prepared;
    m_permittedGeneration.reset();
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    m_lastTransitionSequence.reset();
    m_sessionState->resetForGenerationPurge();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
