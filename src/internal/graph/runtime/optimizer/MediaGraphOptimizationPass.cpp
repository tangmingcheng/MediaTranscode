#include "internal/graph/runtime/optimizer/MediaGraphOptimizationPass.h"

namespace media::ffmpeg::graph {

const char* MediaGraphNoopOptimizationPass::name() const noexcept
{
    return "noop";
}

::media::Status MediaGraphNoopOptimizationPass::run(MediaGraph& graph,
                                                     const MediaGraphOptimizationPolicy&,
                                                     MediaGraphOptimizationReport& report)
{
    report.info(name(),
                "graph accepted without structural rewrite: nodes=" +
                    std::to_string(graph.nodeCount()) +
                    ", edges=" +
                    std::to_string(graph.edgeCount()));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
