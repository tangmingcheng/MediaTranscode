#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAvGenerationPurge final {
    std::uint64_t oldGeneration;
    std::uint64_t nextGeneration;
    std::uint64_t transitionSequence;
};

struct MediaAvGenerationAcknowledgement final {
    MediaAvGenerationParticipant participant;
    std::uint64_t transitionSequence;
    ::media::Status status;
};

enum class MediaAvGenerationReadiness : std::uint8_t {
    Acquiring = 0,
    Locked = 1,
    Reacquire = 2
};

} // namespace media::ffmpeg::graph
