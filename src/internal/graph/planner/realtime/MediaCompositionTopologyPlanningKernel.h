#pragma once
#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"

namespace media::ffmpeg::graph {
// Borrowed only during plan(); candidate and sealed source products use the
// same declarations without constructing a graph or a storage reservation.
struct MediaCompositionTopologySourceFacts final {
    const MediaAvSyncPlan& synchronization;
    const MediaRealtimeEdgePolicySet& policies;
    const MediaCompositionSourceBranchPlan& branches;
    std::span<const MediaSourceInputConnection> inputs;
};
struct MediaCompositionTopologyPlanningProducts final {
    MediaCompositionAggregateConnections aggregate;
    std::vector<MediaCompositionSourceTopology> sources;
    MediaCompositionGraphTopology graph;
};
class MediaCompositionTopologyPlanningKernel final {
public:
    static ::media::Result<MediaCompositionTopologyPlanningProducts> plan(
        std::span<const MediaCompositionTopologySourceFacts> sources,
        const MediaAvContinuousAggregateTopology& aggregate,
        const MediaRealtimeAvOutputRuntimePlan& output);
};
}
