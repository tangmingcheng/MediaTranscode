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

::media::Status MediaProtocolOutputGenerationState::observe(
    std::uint64_t generation)
{
    std::lock_guard lock(m_mutex);
    if (m_plannedIdentity.empty() || generation == 0 ||
        (m_generation && *m_generation != generation)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output generation state rejects an unpurged generation change"));
    }
    m_generation = generation;
    return ::media::Status::success();
}

::media::Status MediaProtocolOutputGenerationState::purge(
    const MediaAvGenerationPurge& purge)
{
    std::lock_guard lock(m_mutex);
    if (m_plannedIdentity.empty() || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        (m_generation && *m_generation != purge.oldGeneration) ||
        (m_lastTransitionSequence &&
         purge.transitionSequence <= *m_lastTransitionSequence)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Protocol output purge requires exact generations and a fresh transition"));
    }
    m_generation = purge.nextGeneration;
    m_lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

void MediaProtocolOutputGenerationState::resetLifecycle() noexcept
{
    std::lock_guard lock(m_mutex);
    m_generation.reset();
    m_lastTransitionSequence.reset();
}

} // namespace media::ffmpeg::graph
