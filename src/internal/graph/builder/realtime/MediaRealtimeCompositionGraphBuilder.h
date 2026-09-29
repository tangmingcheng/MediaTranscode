#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceResourcesPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"
#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasPreparationPlanner.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace media::ffmpeg::graph {

class MediaPreparedVideoCanvas;

struct MediaRealtimeCompositionGraphOptions final {
    std::vector<MediaRealtimeCompositionSourcePlan> sources;
    MediaVideoOutputPlan outputVideo;
    MediaRealtimeAvOutputRuntimePlan outputRuntime;
    MediaAvContinuousAggregateTopology aggregate;
};

struct MediaRealtimeCompositionGraphAssembly final {
    MediaAvSyncRuntimeBinding runtimeBinding;
    std::vector<MediaRealtimeCompositionSourceTargets> sourceTargets;
    std::vector<MediaNodeId> outputMembers;
};

struct MediaRealtimeCompositionGraph final {
    MediaGraph graph;
    MediaRealtimeCompositionGraphAssembly assembly;
};

// Owns the one final logical graph. No runtime binding or mutable graph escapes.
class MediaRealtimeCompositionTopology final {
public:
    MediaRealtimeCompositionTopology(MediaRealtimeCompositionTopology&&) = default;
    MediaRealtimeCompositionTopology& operator=(MediaRealtimeCompositionTopology&&) = default;
    MediaRealtimeCompositionTopology(const MediaRealtimeCompositionTopology&) = delete;
    MediaRealtimeCompositionTopology& operator=(const MediaRealtimeCompositionTopology&) = delete;
    const MediaGraph& graph() const noexcept { return graph_; }
    ::media::Result<std::vector<MediaRealtimeCompositionSourceResources>> planSourceResources() const;
    ::media::Result<MediaVideoCanvasRetentionPlan> planCanvasRetention() const;
    ::media::Result<MediaVideoCanvasPreparationPlan> planCanvasPreparation(
        const MediaVideoCanvasAllocation& allocation) const;
    const MediaAvContinuousAggregateTopology& aggregate() const noexcept { return options_.aggregate; }
private:
    friend class MediaRealtimeCompositionGraphBuilder;
    MediaRealtimeCompositionTopology(MediaGraph graph,
        MediaRealtimeCompositionGraphOptions options,
        std::vector<MediaAvSourceDomainRegistration> sources,
        MediaAvOutputDomainRegistration output,
        std::vector<MediaRealtimeCompositionSourceTargets> targets)
        : graph_(std::move(graph)), options_(std::move(options)),
          sources_(std::move(sources)), output_(std::move(output)), targets_(std::move(targets)) {}
    MediaGraph graph_;
    MediaRealtimeCompositionGraphOptions options_;
    std::vector<MediaAvSourceDomainRegistration> sources_;
    MediaAvOutputDomainRegistration output_;
    std::vector<MediaRealtimeCompositionSourceTargets> targets_;
};

struct MediaRealtimeCompositionPreparedResources final {
    std::vector<std::shared_ptr<MediaPreparedVideoDecoder>> videoDecoders;
    MediaBufferRef videoEncoder;
    std::shared_ptr<MediaPreparedVideoCanvas> canvas;
    MediaVideoCanvasStorage canvasStorage;
};

class MediaRealtimeCompositionGraphBuilder final {
public:
    static ::media::Result<MediaRealtimeCompositionTopology> buildTopology(
        const std::string& prefix, MediaRealtimeCompositionGraphOptions options);
    // Compiles final admission from this exact graph and validates real resources.
    // A topology is not a preparation budget or permission to allocate a pool.
    static ::media::Result<MediaRealtimeCompositionGraph> bind(
        MediaRealtimeCompositionTopology topology,
        const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
        MediaRealtimeCompositionPreparedResources resources);
};

} // namespace media::ffmpeg::graph
