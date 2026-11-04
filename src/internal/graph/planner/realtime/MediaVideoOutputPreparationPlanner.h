#pragma once
#include "internal/graph/planner/realtime/MediaVideoOutputPreparationPlan.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasRetentionPlanner.h"

namespace media::ffmpeg::graph {
class MediaVideoOutputPreparationPlanner final {
public:
    static ::media::Result<MediaVideoOutputPreparationPlan> plan(
        const MediaVideoCanvasRetentionPlan& retention,
        const MediaVideoCanvasAllocation& allocation);
};
} // namespace media::ffmpeg::graph
