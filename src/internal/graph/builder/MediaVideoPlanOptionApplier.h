#pragma once

#include "internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaVideoPlanOptionApplier final {
public:
    static ::media::Result<void> applySelectedPlan(MediaGraph& graph,
                                                   const MediaVideoTranscodeBranchNodes& nodes,
                                                   const MediaPipelinePlan& plan);

private:
    MediaVideoPlanOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
