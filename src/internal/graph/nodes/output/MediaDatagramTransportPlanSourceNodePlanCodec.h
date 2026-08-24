#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"

namespace media::ffmpeg::graph {

class MediaDatagramTransportPlanSourceNodePlanCodec final {
public:
    static ::media::Status apply(
        MediaGraph& graph,
        MediaNodeId nodeId,
        const MediaDatagramTransportPlanTemplate& planTemplate);
    static ::media::Result<MediaDatagramTransportPlanTemplate> decode(
        const MediaNode& node);

private:
    MediaDatagramTransportPlanSourceNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
