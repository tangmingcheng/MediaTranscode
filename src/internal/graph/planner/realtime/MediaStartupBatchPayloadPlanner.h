#pragma once

#include "internal/graph/core/MediaNode.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// Media counts inside one startup release, not wrapper/queue slot counts.
struct MediaStartupBatchPayloadPlan final {
    MediaNodeId coordinator;
    MediaPayloadKind videoPayload;
    std::uint64_t videoUnits;
    std::optional<std::uint64_t> audioPacketUnits;
};

class MediaStartupBatchPayloadPlanner final {
public:
    static ::media::Result<MediaStartupBatchPayloadPlan> plan(const MediaNode& coordinator);
};

} // namespace media::ffmpeg::graph
