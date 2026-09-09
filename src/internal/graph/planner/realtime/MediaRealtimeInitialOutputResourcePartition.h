#pragma once

#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeInitialOutputResourcePartition final {
    MediaFinalGraphResourceLedger shared;
    MediaFinalGraphResourceLedger output;
};

class MediaRealtimeInitialOutputResourcePartitionPlanner final {
public:
    static ::media::Result<MediaRealtimeInitialOutputResourcePartition> plan(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& ledger,
        std::span<const MediaNodeId> outputNodes);
};

} // namespace media::ffmpeg::graph
