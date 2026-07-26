#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaProtocolOutputGenerationCommitReservation::
    MediaProtocolOutputGenerationCommitReservation(
        std::unique_lock<std::mutex> lock) noexcept
    : m_lock(std::move(lock))
{
}

MediaProtocolOutputGenerationState::MediaProtocolOutputGenerationState(
    std::string plannedIdentity)
    : m_plannedIdentity(std::move(plannedIdentity))
{
}

std::string_view
MediaProtocolOutputGenerationState::plannedIdentity() const noexcept
{
    return m_plannedIdentity;
}

::media::Status
MediaProtocolOutputGenerationState::permitActivatedGeneration(
    std::uint64_t generation)
{
    std::lock_guard lock(m_mutex);
    const bool initialActivation =
        !m_permittedGeneration && !m_pendingGeneration &&
        !m_pendingTransitionSequence && !m_lastTransitionSequence;
    const bool plannedRollover =
        m_pendingGeneration && m_pendingTransitionSequence &&
        *m_pendingGeneration == generation;
    if (m_plannedIdentity.empty() || generation == 0 ||
        (!initialActivation && !plannedRollover)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output activation requires the exact planned generation transition"));
    }
    m_permittedGeneration = generation;
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    return ::media::Status::success();
}

::media::Result<MediaProtocolOutputGenerationCommitReservation>
MediaProtocolOutputGenerationState::reserveCommit(
    std::uint64_t generation) const
{
    std::unique_lock lock(m_mutex);
    if (m_plannedIdentity.empty() || generation == 0 ||
        !m_permittedGeneration || *m_permittedGeneration != generation) {
        return ::media::Result<
            MediaProtocolOutputGenerationCommitReservation>::failure(
                ::media::ErrorInfo::cancelled(
                    "Protocol output commit requires the exact permitted generation"));
    }
    return ::media::Result<
        MediaProtocolOutputGenerationCommitReservation>::success(
            MediaProtocolOutputGenerationCommitReservation(std::move(lock)));
}

::media::Status MediaProtocolOutputGenerationState::purge(
    const MediaAvGenerationPurge& purge)
{
    std::lock_guard lock(m_mutex);
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
    return ::media::Status::success();
}

void MediaProtocolOutputGenerationState::resetLifecycle() noexcept
{
    std::lock_guard lock(m_mutex);
    m_permittedGeneration.reset();
    m_pendingGeneration.reset();
    m_pendingTransitionSequence.reset();
    m_lastTransitionSequence.reset();
}

} // namespace media::ffmpeg::graph
