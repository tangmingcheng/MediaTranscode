#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"
#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaPreparedVideoCanvas;

struct MediaRealtimeCompositionGraphOptions final {
    std::vector<MediaRealtimeCompositionSourcePlan> sources;
    std::vector<std::shared_ptr<MediaPreparedVideoDecoder>> preparedVideoDecoders;
    MediaVideoOutputPlan outputVideo;
    MediaRealtimeAvOutputRuntimePlan outputRuntime;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregate;
    MediaBufferRef preparedVideoEncoder;
    std::shared_ptr<MediaPreparedVideoCanvas> preparedCanvas;
};

struct MediaRealtimeCompositionSourceTargets final {
    std::size_t sourceIndex;
    MediaNodeId primaryInput;
    std::optional<MediaNodeId> isolatedAudioInput;
    std::vector<MediaNodeId> sourceMembers;
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

class MediaRealtimeCompositionGraphBuilder final {
public:
    // A composition owns its entire graph; runtime domains cannot omit existing nodes.
    static ::media::Result<MediaRealtimeCompositionGraphAssembly> append(
        MediaGraph& graph, const std::string& prefix,
        MediaRealtimeCompositionGraphOptions options);
    static ::media::Result<MediaRealtimeCompositionGraph> buildGraph(
        const std::string& prefix, MediaRealtimeCompositionGraphOptions options);
};

} // namespace media::ffmpeg::graph
