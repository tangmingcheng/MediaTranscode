#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaAvGenerationParticipantGroup final {
public:
    static ::media::Result<MediaAvGenerationParticipantGroup> create(
        MediaAvGenerationParticipantPlan plan);

    ::media::Status registerChild(
        std::string identity,
        std::shared_ptr<MediaAvGenerationPurgeTarget> child);
    ::media::Status seal();
    void bindPurgeProgressWakeups(
        std::shared_ptr<const std::vector<std::shared_ptr<MediaNodeWakeup>>> wakeups);
    ::media::Result<std::optional<MediaAvGenerationAcknowledgement>> purgeAll(
        const MediaAvGenerationPurge& purge);

private:
    explicit MediaAvGenerationParticipantGroup(
        MediaAvGenerationParticipantPlan plan);

    MediaAvGenerationParticipantPlan m_plan;
    std::map<std::string, std::shared_ptr<MediaAvGenerationPurgeTarget>> m_children;
    std::optional<std::uint64_t> m_lastTransitionSequence;
    std::optional<MediaAvGenerationPurge> m_pendingPurge;
    std::vector<bool> m_completedChildren;
    bool m_acknowledged = false;
    bool m_sealed = false;
};

} // namespace media::ffmpeg::graph
