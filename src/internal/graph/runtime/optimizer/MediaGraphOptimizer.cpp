#include "internal/graph/runtime/optimizer/MediaGraphOptimizer.h"

#include "internal/graph/runtime/optimizer/passes/MediaNodeFusionPass.h"
#include "internal/graph/runtime/optimizer/passes/MediaRedundantTransferEliminationPass.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaGraphOptimizer::MediaGraphOptimizer()
{
    addPass(std::make_unique<MediaGraphNoopOptimizationPass>());
    addPass(std::make_unique<MediaNodeFusionPass>());
    addPass(std::make_unique<MediaRedundantTransferEliminationPass>());
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
        report.info("optimizer", "optimization disabled");
        return ::media::Result<MediaGraphOptimizerResult>::success(
            MediaGraphOptimizerResult{ std::move(graph), std::move(report) });
    }

    for (const auto& pass : m_passes) {
        if (!pass) {
            continue;
        }

        auto status = pass->run(graph, policy, report);
        if (!status) {
            report.error(pass->name(), status.error().describe());
            return ::media::Result<MediaGraphOptimizerResult>::failure(status.error());
        }
    }

    return ::media::Result<MediaGraphOptimizerResult>::success(
        MediaGraphOptimizerResult{ std::move(graph), std::move(report) });
}

} // namespace media::ffmpeg::graph
