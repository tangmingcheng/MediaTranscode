#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphOptimizationPolicy.h"
#include "internal/graph/runtime/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizationReport.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaGraphExecutionPlan {
    MediaGraph graph;
    MediaGraphOptimizationPolicy policy;
    MediaGraphOptimizationReport optimizationReport;
};

class MediaGraphCompiler final {
public:
    ::media::Result<MediaGraphExecutionPlan> compile(
        MediaGraph graph,
        const MediaGraphOptimizationPolicy& policy = {});
};

} // namespace media::ffmpeg::graph
