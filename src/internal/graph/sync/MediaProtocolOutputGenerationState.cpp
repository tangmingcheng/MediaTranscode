#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <utility>

namespace media::ffmpeg::graph {

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
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    std::lock_guard lock(m_mutex);
    const bool initialActivation =
        !m_permittedGeneration && !m_pendingGeneration &&
        !m_lastTransitionSequence && transitionSequence == 0;
    const bool plannedRollover =
        m_pendingGeneration && m_pendingTransitionSequence &&
        *m_pendingGeneration == generation &&
        *m_pendingTransitionSequence == transitionSequence;
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

::media::Status
MediaProtocolOutputGenerationState::validateCommitGeneration(
    std::uint64_t generation) const
{
    std::lock_guard lock(m_mutex);
    if (m_plannedIdentity.empty() || generation == 0 ||
        !m_permittedGeneration || *m_permittedGeneration != generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "Protocol output commit requires the exact permitted generation"));
    }
    return ::media::Status::success();
}

std::optional<std::uint64_t>
MediaProtocolOutputGenerationState::activationTransitionSequence(
    std::uint64_t generation) const noexcept
{
    std::lock_guard lock(m_mutex);
    if (!m_pendingGeneration || !m_pendingTransitionSequence ||
        *m_pendingGeneration != generation) {
        return std::nullopt;
    }
    return m_pendingTransitionSequence;
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
