#pragma once

#include "internal/graph/model/MediaGraphPayloadRetentionGrowth.h"
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

namespace media::ffmpeg::graph {
class MediaRealtimeOutputSourceRetentionPlanner final {
public:
    static ::media::Result<MediaGraphPayloadRetentionGrowth> plan(
        const MediaGraph& graph,
        std::span<const MediaNodeId> outputNodes,
        MediaNodeId sourceFanout,
        const MediaRealtimeGraphResourceLedgerPlan& planning,
        const MediaFinalGraphResourceLedger& outputResources);
};
} // namespace media::ffmpeg::graph
