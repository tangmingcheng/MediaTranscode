#include "internal/graph/runtime/optimizer/passes/MediaGraphIrLoweringPass.h"

#include "internal/graph/runtime/compiler/MediaGraphInstructionLowerer.h"

namespace media::ffmpeg::graph {

const char* MediaGraphIrLoweringPass::name() const noexcept
{
    return "ir-lowering";
}

::media::Status MediaGraphIrLoweringPass::run(MediaGraph& graph,
                                               const MediaGraphOptimizationPolicy&,
                                               MediaGraphOptimizationReport& report)
{
    auto plan = MediaGraphInstructionLowerer::lower(graph);
    if (!plan) {
        return ::media::Status::failure(plan.error());
    }

    report.info(name(), "lowered graph to instruction plan: instructions=" +
                           std::to_string(plan.value().size()));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
