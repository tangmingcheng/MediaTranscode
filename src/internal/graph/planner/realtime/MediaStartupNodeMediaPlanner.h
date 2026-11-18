#pragma once

#include "internal/graph/planner/realtime/MediaStartupEventMediaPlanner.h"

#include <span>

namespace media::ffmpeg::graph {

// Distinct media retained inside one node; excludes its incident edge queues.
struct MediaStartupNodeMediaBound final {
    MediaNodeId node;
    MediaNodeId coordinator;
    MediaPayloadKind videoPayload;
    std::optional<std::uint64_t> videoUnits;
    std::optional<std::uint64_t> audioPacketUnits;
};

class MediaStartupNodeMediaPlanner final {
public:
    static ::media::Result<std::vector<MediaStartupNodeMediaBound>> plan(
        const MediaGraph& graph, const MediaStartupBatchPayloadPlan& batch,
        std::span<const MediaStartupEventMediaBound> events);
};

} // namespace media::ffmpeg::graph
