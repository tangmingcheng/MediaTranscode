#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

namespace media::ffmpeg::graph {

class MediaRealtimeGraphResourceBinding final {
public:
    static ::media::Status apply(MediaGraph& graph, MediaNodeId codecResolver,
        const MediaFinalGraphResourceLedger& ledger);
};

} // namespace media::ffmpeg::graph
