#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlannerReport.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/gpu/MediaGpuGraphCommand.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaGraphGpuPlannerResult {
    MediaGpuGraphCommandList commands;
    MediaGraphPlannerReport report;
};

class MediaGraphGpuPlanner final {
public:
    static ::media::Result<MediaGraphGpuPlannerResult> plan(
        const MediaGraph& graph,
        const MediaGraphPlanningPolicy& policy = {});

};

} // namespace media::ffmpeg::graph
