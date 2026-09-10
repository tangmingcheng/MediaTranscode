#pragma once
#include "internal/graph/core/MediaNodeId.h"
#include <cstdint>

namespace media::ffmpeg::graph {
// Planner-owned additional live-reference demand on an existing producer account.
struct MediaGraphPayloadRetentionGrowth final {
    MediaNodeId sourceAccountProducer;
    std::uint64_t additionalBytes;
    std::uint64_t additionalObjects;
};
} // namespace media::ffmpeg::graph
