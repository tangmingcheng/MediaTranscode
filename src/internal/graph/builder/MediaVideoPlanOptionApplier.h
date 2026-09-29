#pragma once

#include "internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"

namespace media::ffmpeg::graph {

class MediaVideoPlanOptionApplier final {
public:
    static ::media::Result<void> applySelectedPlan(MediaGraph& graph,
                                                   const MediaVideoTranscodeBranchNodes& nodes,
                                                   const MediaPipelinePlan& plan);

    static ::media::Result<void> applyEncoderPlan(
        MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
        const MediaPipelineStagePlan& encoder, MediaVideoEncoderAbortPolicy abortPolicy);

    static ::media::Result<void> applySourcePlan(
        MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
        const MediaVideoSourcePlan& plan, int sourceStreamIndex,
        MediaRational frameRate, const std::optional<MediaRational>& maximumDuplicationGap);

    static ::media::Result<void> applyFilterExecutionPlan(
        MediaGraph& graph, MediaNodeId node, const MediaVideoFilterExecutionPlan& plan);

private:
    MediaVideoPlanOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
