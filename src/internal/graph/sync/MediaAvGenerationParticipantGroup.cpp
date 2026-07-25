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

::media::Result<MediaAvGenerationAcknowledgement>
MediaAvGenerationParticipantGroup::purgeAll(
    const MediaAvGenerationPurge& purge)
{
    if (!m_sealed || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        (m_lastTransitionSequence &&
         purge.transitionSequence <= *m_lastTransitionSequence)) {
        return ::media::Result<MediaAvGenerationAcknowledgement>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation purge requires a sealed group and a fresh valid transition"));
    }
    m_lastTransitionSequence = purge.transitionSequence;
    std::optional<::media::ErrorInfo> firstFailure;
    for (const auto& identity : m_plan.requiredChildren) {
        auto status = m_children.at(identity)->purge(purge);
        if (!status && !firstFailure) firstFailure = status.error();
    }
    if (firstFailure) {
        return ::media::Result<MediaAvGenerationAcknowledgement>::failure(
            std::move(*firstFailure));
    }
    return ::media::Result<MediaAvGenerationAcknowledgement>::success(
        MediaAvGenerationAcknowledgement{
            m_plan.participant,
            purge.transitionSequence,
            ::media::Status::success()});
}

} // namespace media::ffmpeg::graph
