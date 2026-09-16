#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"

#include <algorithm>
#include <set>
#include <utility>

namespace media::ffmpeg::graph {

MediaAvGenerationParticipantGroup::MediaAvGenerationParticipantGroup(
    MediaAvGenerationParticipantPlan plan)
    : m_plan(std::move(plan))
{
}

::media::Result<MediaAvGenerationParticipantGroup>
MediaAvGenerationParticipantGroup::create(
    MediaAvGenerationParticipantPlan plan)
{
    std::set<std::string> identities;
    for (const auto& identity : plan.requiredChildren) {
        if (identity.empty() || !identities.insert(identity).second) {
            return ::media::Result<MediaAvGenerationParticipantGroup>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Generation participant requires unique non-empty child identities"));
        }
    }
    if (identities.empty()) {
        return ::media::Result<MediaAvGenerationParticipantGroup>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation participant requires at least one child"));
    }
    return ::media::Result<MediaAvGenerationParticipantGroup>::success(
        MediaAvGenerationParticipantGroup(std::move(plan)));
}

::media::Status MediaAvGenerationParticipantGroup::registerChild(
    std::string identity,
    std::shared_ptr<MediaAvGenerationPurgeTarget> child)
{
    if (m_sealed || identity.empty() || !child) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation child registration requires an unsealed group, identity, and target"));
    }
    const auto planned = std::find(
        m_plan.requiredChildren.begin(), m_plan.requiredChildren.end(), identity);
    if (planned == m_plan.requiredChildren.end() || m_children.contains(identity)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation child registration must match one unregistered planned identity"));
    }
    m_children.emplace(std::move(identity), std::move(child));
    return ::media::Status::success();
}

::media::Status MediaAvGenerationParticipantGroup::seal()
{
    if (m_sealed || m_children.size() != m_plan.requiredChildren.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation participant can seal exactly once with its complete child set"));
    }
    m_sealed = true;
    return ::media::Status::success();
}

void MediaAvGenerationParticipantGroup::bindPurgeProgressWakeups(
    std::shared_ptr<const std::vector<std::shared_ptr<MediaNodeWakeup>>> wakeups)
{
    for (const auto& [identity, child] : m_children)
        child->bindPurgeProgressWakeups(wakeups);
}

::media::Result<std::optional<MediaAvGenerationAcknowledgement>>
MediaAvGenerationParticipantGroup::purgeAll(
    const MediaAvGenerationPurge& purge)
{
    using Result = ::media::Result<std::optional<MediaAvGenerationAcknowledgement>>;
    const bool continuing = m_pendingPurge &&
        m_pendingPurge->oldGeneration == purge.oldGeneration &&
        m_pendingPurge->nextGeneration == purge.nextGeneration &&
        m_pendingPurge->transitionSequence == purge.transitionSequence;
    if (!m_sealed || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        (!continuing && ((m_pendingPurge && !m_acknowledged) ||
         (m_lastTransitionSequence &&
          purge.transitionSequence <= *m_lastTransitionSequence)))) {
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation purge requires a sealed group and a fresh valid transition"));
    }
    if (!continuing) {
        m_lastTransitionSequence = purge.transitionSequence;
        m_pendingPurge = purge;
        m_completedChildren.assign(m_plan.requiredChildren.size(), false);
        m_acknowledged = false;
    }
    if (m_acknowledged) return Result::success(std::nullopt);
    std::optional<::media::ErrorInfo> firstFailure;
    bool pending = false;
    for (std::size_t index = 0; index < m_plan.requiredChildren.size(); ++index) {
        if (m_completedChildren[index]) continue;
        const auto& identity = m_plan.requiredChildren[index];
        auto status = m_children.at(identity)->purge(purge);
        if (status) m_completedChildren[index] = true;
        else if (status.error().code == ::media::ErrorCode::WouldBlock) pending = true;
        else if (!firstFailure) firstFailure = status.error();
    }
    if (firstFailure) {
        return Result::failure(std::move(*firstFailure));
    }
    if (pending) return Result::success(std::nullopt);
    m_acknowledged = true;
    return Result::success(
        MediaAvGenerationAcknowledgement{
            m_plan.participant,
            purge.transitionSequence,
            ::media::Status::success()});
}

} // namespace media::ffmpeg::graph
