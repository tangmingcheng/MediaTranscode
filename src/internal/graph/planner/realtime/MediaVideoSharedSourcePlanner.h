#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"

namespace media::ffmpeg::graph {
class MediaVideoSharedSourcePlanner final {
public:
    static ::media::Result<MediaVideoSharedSourcePlan> plan(
        const MediaPipelineChainPlan& chain, const MediaInputVideoStreamInfo& source);
};
} // namespace media::ffmpeg::graph
