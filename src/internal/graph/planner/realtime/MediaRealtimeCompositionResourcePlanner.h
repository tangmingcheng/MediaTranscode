#pragma once
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"
#include "internal/graph/model/MediaCompositionGraphIdentity.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceResourcesPlanner.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasRetentionPlanner.h"
#include "internal/graph/planner/audio/MediaAudioEncoderFifoRetentionPlan.h"

namespace media::ffmpeg::graph {
class MediaRealtimeCompositionResourcePlanner final {
public:
    static ::media::Result<MediaFinalGraphResourceAdmissionPlan> plan(
        const MediaGraph& graph, const MediaCompositionGraphTopology& projection,
        const MediaCompositionGraphIdentity& identity, std::span<const MediaRealtimeCompositionSourceResources> sources,
        MediaNodeId aggregateNode, MediaNodeId audioAggregate, const MediaAvContinuousAggregateTopology& aggregate,
        const MediaVideoCanvasStorage& canvas, const MediaVideoCanvasRetentionPlan& retention,
        const MediaPreparedRealtimeEmissionSet& output,
        const MediaAudioEncoderFifoRetentionPlan& outputFifo,
        const MediaRealtimeDeploymentLatencyBudget& latency);
};
} // namespace media::ffmpeg::graph
