#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"

#include <set>
#include <utility>

namespace media::ffmpeg::graph {

MediaAvGenerationParticipantAssembler::
    MediaAvGenerationParticipantAssembler(
        MediaAvGenerationTransitionPlan plan)
    : m_plan(std::move(plan))
{
}

::media::Result<MediaAvGenerationParticipantAssembler>
MediaAvGenerationParticipantAssembler::create(
    MediaAvGenerationTransitionPlan plan)
{
    if (plan.participants.empty()) {
        return ::media::Result<
            MediaAvGenerationParticipantAssembler>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation participant assembly requires a planned participant set"));
    }

    std::set<MediaAvGenerationParticipant> participants;
    MediaAvGenerationParticipantAssembler assembler(std::move(plan));
    for (const auto& participant : assembler.m_plan.participants) {
        if (!participants.insert(participant.participant).second) {
            return ::media::Result<
                MediaAvGenerationParticipantAssembler>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Generation participant assembly rejects duplicate participants"));
        }
        auto group = MediaAvGenerationParticipantGroup::create(participant);
        if (!group) {
            return ::media::Result<
                MediaAvGenerationParticipantAssembler>::failure(
                group.error());
        }
        assembler.m_groups.emplace(
            participant.participant, std::move(group).value());
    }
    return ::media::Result<
        MediaAvGenerationParticipantAssembler>::success(
        std::move(assembler));
}

::media::Status MediaAvGenerationParticipantAssembler::registerTarget(
    MediaAvGenerationParticipant participant,
    MediaAvGenerationPurgeRegistration registration)
{
    if (m_sealed || registration.identity.empty() || !registration.target) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation purge registration requires an unsealed assembler, identity, and target"));
    }
    auto group = m_groups.find(participant);
    if (group == m_groups.end()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation purge registration names an unplanned participant"));
    }
    return group->second.registerChild(
        std::move(registration.identity),
        std::move(registration.target));
}

::media::Result<std::vector<MediaAvGenerationParticipantGroup>>
MediaAvGenerationParticipantAssembler::seal()
{
    if (m_sealed) {
        return ::media::Result<
            std::vector<MediaAvGenerationParticipantGroup>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation participant assembly seals exactly once"));
    }
    m_sealed = true;

    std::vector<MediaAvGenerationParticipantGroup> groups;
    groups.reserve(m_plan.participants.size());
    for (const auto& participant : m_plan.participants) {
        auto found = m_groups.find(participant.participant);
        if (found == m_groups.end()) {
            return ::media::Result<
                std::vector<MediaAvGenerationParticipantGroup>>::failure(
                ::media::ErrorInfo::internalError(
                    "Generation participant assembly lost a planned group"));
        }
        if (auto sealed = found->second.seal(); !sealed) {
            return ::media::Result<
                std::vector<MediaAvGenerationParticipantGroup>>::failure(
                sealed.error());
        }
    }
    for (const auto& participant : m_plan.participants) {
        auto found = m_groups.find(participant.participant);
        groups.push_back(std::move(found->second));
    }
    return ::media::Result<
        std::vector<MediaAvGenerationParticipantGroup>>::success(
        std::move(groups));
}

} // namespace media::ffmpeg::graph
