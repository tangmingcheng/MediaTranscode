#include "internal/graph/runtime/optimizer/passes/MediaGraphIrLoweringPass.h"

namespace media::ffmpeg::graph {

const char* MediaGraphIrLoweringPass::name() const noexcept
{
    return "ir-lowering";
}

::media::Status MediaGraphIrLoweringPass::run(MediaGraph& graph,
                                               const MediaGraphOptimizationPolicy&,
                                               MediaGraphOptimizationReport& report)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphIrLoweringPass is unsupported: lowered instructions are not consumed by the runtime"));
}

} // namespace media::ffmpeg::graph
