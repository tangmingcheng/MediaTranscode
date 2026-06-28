#include "internal/graph/runtime/optimizer/MediaGraphCompiler.h"

#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphExecutionPlan> MediaGraphCompiler::compile(
    MediaGraph graph,
    const MediaGraphOptimizationPolicy& policy)
{
    auto validation = MediaGraphValidation::validate(graph);
    if (!validation.ok()) {
        return ::media::Result<MediaGraphExecutionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaGraphCompiler failed: graph validation has " +
                std::to_string(validation.errorCount()) + " error(s)"));
    }

    MediaGraphOptimizer optimizer;
    auto optimized = optimizer.optimize(std::move(graph), policy);
    if (!optimized) {
        return ::media::Result<MediaGraphExecutionPlan>::failure(optimized.error());
    }

    MediaGraphExecutionPlan plan;
    plan.graph = std::move(optimized.value().graph);
    plan.policy = policy;
    plan.optimizationReport = std::move(optimized.value().report);
    return ::media::Result<MediaGraphExecutionPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
