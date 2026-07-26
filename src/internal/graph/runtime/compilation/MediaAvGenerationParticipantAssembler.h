#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvGenerationPurgeRegistration final {
    std::string identity;
    std::shared_ptr<MediaAvGenerationPurgeTarget> target;
};

class MediaAvGenerationParticipantAssembler final {
public:
    static ::media::Result<MediaAvGenerationParticipantAssembler> create(
        MediaAvGenerationTransitionPlan plan);

    ::media::Status registerTarget(
        MediaAvGenerationParticipant participant,
        MediaAvGenerationPurgeRegistration registration);
    ::media::Result<std::vector<MediaAvGenerationParticipantGroup>> seal();

private:
    explicit MediaAvGenerationParticipantAssembler(
        MediaAvGenerationTransitionPlan plan);

    MediaAvGenerationTransitionPlan m_plan;
    std::map<MediaAvGenerationParticipant, MediaAvGenerationParticipantGroup>
        m_groups;
    bool m_sealed = false;
};

} // namespace media::ffmpeg::graph
