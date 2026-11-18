#pragma once
#include "internal/graph/model/MediaNodeKind.h"
#include "media_transcode/Result.h"
#include <cstdint>
#include <optional>
#include <span>
namespace media::ffmpeg::graph {
struct MediaNodeReferenceRetentionFacts final {
    MediaNodeKind kind;
    // One entry per incident edge; a self-edge is included only once.
    std::span<const std::uint64_t> incidentCapacities;
    std::optional<std::uint64_t> rtpAcquiringCapacity;
    std::optional<std::uint64_t> rawDescriptorCapacity;
    // Zero is authoritative for a filter without SourceFrame input retention.
    std::optional<std::uint64_t> filterRetainedInputs;
};
class MediaNodeReferenceRetentionPlanner final {
public:
    static ::media::Result<std::uint64_t> plan(const MediaNodeReferenceRetentionFacts& facts);
};
}
