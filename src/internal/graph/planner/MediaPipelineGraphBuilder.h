#pragma once

#include "internal/graph/planner/MediaPipelinePlanner.h"

namespace media::ffmpeg::graph {

class MediaPipelineGraphBuilder final {
public:
    static ::media::Result<MediaPipelineGraphBuildResult> buildVideoFileTranscodeGraph(
        MediaPipelinePlan plan);

    static ::media::Status applyVideoPlanToGraph(MediaGraph& graph,
                                                 MediaNodeId videoDecodeNode,
                                                 MediaNodeId videoFilterNode,
                                                 MediaNodeId videoEncodeNode,
                                                 const MediaPipelinePlan& plan);

private:
    MediaPipelineGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
