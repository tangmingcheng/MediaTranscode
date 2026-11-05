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
// Fixed output facts are owned by the candidate workspace, never borrowed
// across a freeze. Their nested storage still requires workspace admission.
struct MediaCompositionOutputTopologyDeclarations final {
    MediaRealtimeEdgePolicySet policies;
    MediaVideoBranchShape videoShape;
    std::vector<MediaVideoBranchConnection> videoConnections;
    MediaAudioBranchShape audioShape;
    std::vector<MediaAudioBranchConnection> audioConnections;
    MediaOutputSegmentDeclaration scheduler;
    MediaOutputSegmentDeclaration protocol;
    MediaOutputSegmentDeclaration execution;
};
struct MediaCompositionTopologyPlanningProducts final {
    MediaCompositionAggregateConnections aggregate;
    std::vector<MediaCompositionSourceTopology> sources;
    MediaCompositionGraphTopology graph;
    std::vector<MediaRealtimeEdgePolicySet> sourcePolicies;
    MediaCompositionOutputTopologyDeclarations output;
};
class MediaCompositionTopologyPlanningKernel final {
public:
    // Call after source branch/input products have been refreshed. Output
    // declarations are fixed at plan(); failure invalidates all products.
    static ::media::Status rebind(
        std::span<const MediaCompositionTopologySourceFacts> sources,
        const MediaAvContinuousAggregateTopology& aggregate,
        MediaCompositionTopologyPlanningProducts& products);
    static ::media::Result<MediaCompositionTopologyPlanningProducts> plan(
        std::span<const MediaCompositionTopologySourceFacts> sources,
        const MediaAvContinuousAggregateTopology& aggregate,
        const MediaRealtimeAvOutputRuntimePlan& output);
};
}
