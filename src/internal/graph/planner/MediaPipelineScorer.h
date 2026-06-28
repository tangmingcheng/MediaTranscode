#pragma once

#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <vector>

namespace media::ffmpeg::graph {

class MediaPipelineScorer final {
public:
    static MediaPipelineChainPlan scoreChain(MediaPipelineChainPlan chain,
                                             const MediaPipelinePlannerOptions& options);

    static std::vector<MediaPipelineChainPlan> scoreAndSortChains(
        std::vector<MediaPipelineChainPlan> chains,
        const MediaPipelinePlannerOptions& options);

private:
    MediaPipelineScorer() = default;
};

} // namespace media::ffmpeg::graph
