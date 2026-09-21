#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaAvPublishedGeneration final {
    std::uint64_t generation;
};

struct MediaAvUnpublishedAcquisition final {
    std::uint64_t generation;
    std::uint64_t completedTransitionSequence;
};

using MediaAvTransitionOrigin = std::variant<
    MediaAvPublishedGeneration, MediaAvUnpublishedAcquisition>;

struct MediaAvGenerationPurge final {
    std::uint64_t oldGeneration;
    std::uint64_t nextGeneration;
    std::uint64_t transitionSequence;
    std::uint64_t publishedGeneration;
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
