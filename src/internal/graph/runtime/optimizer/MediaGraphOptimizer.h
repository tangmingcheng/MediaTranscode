#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphOptimizationPolicy.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizationPass.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizationReport.h"
#include "media_transcode/Result.h"

#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphOptimizerResult {
    MediaGraph graph;
    MediaGraphOptimizationReport report;
};

class MediaGraphOptimizer final {
public:
    MediaGraphOptimizer();

    void addPass(std::unique_ptr<MediaGraphOptimizationPass> pass);
    void clearPasses();

    ::media::Result<MediaGraphOptimizerResult> optimize(
        MediaGraph graph,
        const MediaGraphOptimizationPolicy& policy = {});

private:
    std::vector<std::unique_ptr<MediaGraphOptimizationPass>> m_passes;
};

} // namespace media::ffmpeg::graph
