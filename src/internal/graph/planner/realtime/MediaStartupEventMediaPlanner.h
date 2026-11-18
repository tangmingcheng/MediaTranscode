#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaStartupRoutePlan.h"
#include "internal/graph/planner/realtime/MediaStartupBatchPayloadPlanner.h"

namespace media::ffmpeg::graph {

struct MediaStartupEventMediaBound final {
    MediaEdgeId edge;
    MediaNodeId coordinator;
    MediaStartupEventMediaForm form;
    MediaPayloadKind videoPayload;
    std::optional<std::uint64_t> videoUnits;
    std::optional<std::uint64_t> audioPacketUnits;
    std::uint64_t wrapperSlots;
};

// Describes media inside event queues; these are not unique-payload totals.
class MediaStartupEventMediaPlanner final {
public:
    static ::media::Result<std::vector<MediaStartupEventMediaBound>> plan(
        const MediaGraph& graph, const MediaStartupBatchPayloadPlan& batch);
};

} // namespace media::ffmpeg::graph
