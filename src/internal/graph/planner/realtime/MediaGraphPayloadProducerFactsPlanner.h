#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphPayloadProducerFact.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include <span>

namespace media::ffmpeg::graph {

class MediaGraphPayloadProducerFactsPlanner final {
public:
    // Existing single-input planning product; no whole-ledger summation for
    // composition. Multi-owner callers must provide each producer's own facts.
    static ::media::Result<std::vector<MediaGraphPayloadProducerFact>> plan(
        const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& ledger,
        std::span<const MediaNodeId> selectedNodes);
};

} // namespace media::ffmpeg::graph
