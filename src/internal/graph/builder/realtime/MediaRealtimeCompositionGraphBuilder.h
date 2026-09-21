#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionGraphOptions final {
    std::vector<MediaRealtimeRtpTranscodePlan> sources;
    MediaPipelinePlan outputVideo;
    MediaVideoTranscodeParameters outputVideoParameters;
    MediaRealtimeAvSyncRuntimePlan outputRuntime;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregate;
    MediaBufferRef preparedVideoEncoder;
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
