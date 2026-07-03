#pragma once

#include "internal/graph/builder/segments/MediaAudioEncodeBranchNodes.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAudioPlanOptionApplier final {
public:
    static ::media::Result<void> applySelectedPlan(MediaGraph& graph,
                                                   const MediaAudioEncodeBranchNodes& nodes,
                                                   const MediaAudioPipelinePlan& plan);

private:
    MediaAudioPlanOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
