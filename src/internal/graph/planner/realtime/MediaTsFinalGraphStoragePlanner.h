#pragma once
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include <span>

namespace media::ffmpeg::graph {
class MediaTsFinalGraphStoragePlanner final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> plan(
        const MediaGraph& graph, MediaNodeId source,
        const MediaDatagramTransportPlanTemplate& base);
    static ::media::Status apply(MediaGraph& graph, std::span<const MediaNodeId> scope);
};
} // namespace media::ffmpeg::graph
