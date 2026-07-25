#include "internal/graph/runtime/optimizer/MediaGraphOptimizer.h"

#include "internal/graph/runtime/optimizer/passes/MediaGraphIrLoweringPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaHardwarePipelineFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaNodeFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaRedundantTransferEliminationPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaSimdSchedulingPass.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaGraphOptimizer::MediaGraphOptimizer()
{
    addPass(std::make_unique<MediaGraphNoopOptimizationPass>());
    addPass(std::make_unique<MediaGraphIrLoweringPass>());
    addPass(std::make_unique<MediaNodeFusionPass>());
    addPass(std::make_unique<MediaRedundantTransferEliminationPass>());
    addPass(std::make_unique<MediaHardwarePipelineFusionPass>());
    addPass(std::make_unique<MediaSimdSchedulingPass>());
}

void MediaGraphOptimizer::addPass(std::unique_ptr<MediaGraphOptimizationPass> pass)
{
    if (pass) {
        m_passes.push_back(std::move(pass));
    }
}

void MediaGraphOptimizer::clearPasses()
{
    m_passes.clear();
}

::media::Result<MediaGraphOptimizerResult> MediaGraphOptimizer::optimize(
    MediaGraph graph,
    const MediaGraphOptimizationPolicy& policy)
{
    MediaGraphOptimizationReport report;

    if (!policy.enabled()) {
        report.info("optimizer", "no graph optimization requested");
        return ::media::Result<MediaGraphOptimizerResult>::success(
            MediaGraphOptimizerResult{ std::move(graph), std::move(report) });
    }

    return ::media::Result<MediaGraphOptimizerResult>::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphOptimizer is unsupported: no optimizer pass currently applies an executable graph rewrite"));

}

} // namespace media::ffmpeg::graph
