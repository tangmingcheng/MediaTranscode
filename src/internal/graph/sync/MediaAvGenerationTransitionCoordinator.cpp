#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace media::ffmpeg::graph {

MediaAvGenerationTransitionCoordinator::MediaAvGenerationTransitionCoordinator(
    MediaAvGenerationTransitionPlan plan)
    : m_plan(std::move(plan))
{
}

::media::Result<MediaAvGenerationTransitionCoordinator>
MediaAvGenerationTransitionCoordinator::create(
    MediaAvGenerationTransitionPlan plan)
{
    if (plan.participants.empty() ||
        plan.acknowledgementTimeout.nanoseconds() <= 0 ||
        plan.terminalDrainWindow.nanoseconds() <= 0) {
        return ::media::Result<MediaAvGenerationTransitionCoordinator>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation transition requires participants and valid durations"));
    }
    std::set<MediaAvGenerationParticipant> participants;
    for (const auto& participant : plan.participants) {
        if (participant.requiredChildren.empty() ||
            !participants.insert(participant.participant).second) {
            return ::media::Result<MediaAvGenerationTransitionCoordinator>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Generation transition requires unique participants with children"));
        }
        std::set<std::string> children;
        for (const auto& child : participant.requiredChildren) {
            if (child.empty() || !children.insert(child).second) {
                return ::media::Result<MediaAvGenerationTransitionCoordinator>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Generation participant child identities must be unique and non-empty"));
            }
        }
    }
    return ::media::Result<MediaAvGenerationTransitionCoordinator>::success(
        MediaAvGenerationTransitionCoordinator(std::move(plan)));
}

::media::Status MediaAvGenerationTransitionCoordinator::permitInitial(
    std::uint64_t generation)
{
    if (m_poisoned || generation == 0 || m_currentGeneration || m_active ||
        m_completed || m_permitted) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Initial generation permission requires a fresh coordinator"));
    }
    m_currentGeneration = generation;
    m_permitted = true;
    return ::media::Status::success();
}

::media::Result<MediaAvGenerationPurge>
MediaAvGenerationTransitionCoordinator::begin(
    std::uint64_t oldGeneration,
    std::uint64_t nextGeneration)
{
    if (m_poisoned || !m_permitted || !m_currentGeneration || m_active ||
        m_completed || oldGeneration != *m_currentGeneration ||
        nextGeneration <= oldGeneration ||
        m_nextSequence == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaAvGenerationPurge>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation transition requires the permitted current generation and a newer target"));
    }
    m_permitted = false;
    const MediaAvGenerationPurge purge{
        oldGeneration, nextGeneration, m_nextSequence++};
    m_active.emplace(ActiveTransition{
        purge, std::vector<bool>(m_plan.participants.size(), false)});
    return ::media::Result<MediaAvGenerationPurge>::success(purge);
}

std::optional<std::size_t>
MediaAvGenerationTransitionCoordinator::participantIndex(
    MediaAvGenerationParticipant participant) const noexcept
{
    for (std::size_t index = 0; index < m_plan.participants.size(); ++index) {
        if (m_plan.participants[index].participant == participant) return index;
    }
    return std::nullopt;
}

::media::Result<bool> MediaAvGenerationTransitionCoordinator::acknowledge(
    MediaAvGenerationAcknowledgement acknowledgement)
{
    if (m_poisoned || !m_active ||
        acknowledgement.transitionSequence !=
            m_active->purge.transitionSequence) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation acknowledgement does not match the active transition"));
    }
    const auto index = participantIndex(acknowledgement.participant);
    if (!index || m_active->acknowledged[*index]) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation acknowledgement participant is unplanned or duplicated"));
    }
    if (!acknowledgement.status) {
        const auto error = acknowledgement.status.error();
        poison(error);
        return ::media::Result<bool>::failure(error);
    }
    m_active->acknowledged[*index] = true;
    const bool complete = std::all_of(
        m_active->acknowledged.begin(), m_active->acknowledged.end(),
        [](bool value) { return value; });
    if (!complete) return ::media::Result<bool>::success(false);
    m_completed.emplace(CompletedTransition{
        m_active->purge.transitionSequence,
        m_active->purge.nextGeneration});
    m_active.reset();
    return ::media::Result<bool>::success(true);
}

::media::Status MediaAvGenerationTransitionCoordinator::checkTimeout(
    MediaRunningTime elapsedSinceBegin)
{
    if (m_poisoned || !m_active || elapsedSinceBegin.nanoseconds() < 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation timeout check requires an active transition and non-negative elapsed time"));
    }
    if (elapsedSinceBegin < m_plan.acknowledgementTimeout) {
        return ::media::Status::success();
    }
    return poison(::media::ErrorInfo::cancelled(
        "Generation transition acknowledgement timeout"));
}

::media::Status
MediaAvGenerationTransitionCoordinator::publishCompletedGeneration(
    std::uint64_t completedTransitionSequence,
    std::uint64_t expectedNextGeneration)
{
    if (m_poisoned || !m_completed || m_active || m_permitted ||
        completedTransitionSequence != m_completed->sequence ||
        expectedNextGeneration != m_completed->nextGeneration) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation publication requires the matching completed transition"));
    }
    m_currentGeneration = m_completed->nextGeneration;
    m_completed.reset();
    m_permitted = true;
    return ::media::Status::success();
}

::media::Status MediaAvGenerationTransitionCoordinator::poison(
    ::media::ErrorInfo error)
{
    m_permitted = false;
    m_active.reset();
    m_completed.reset();
    m_poisoned = true;
    return ::media::Status::failure(std::move(error));
}

void MediaAvGenerationTransitionCoordinator::abort() noexcept
{
    m_permitted = false;
    m_active.reset();
    m_completed.reset();
    m_poisoned = true;
}

bool MediaAvGenerationTransitionCoordinator::outputPermitted(
    std::uint64_t generation) const noexcept
{
    return !m_poisoned && m_permitted && m_currentGeneration &&
        generation == *m_currentGeneration;
}

} // namespace media::ffmpeg::graph
