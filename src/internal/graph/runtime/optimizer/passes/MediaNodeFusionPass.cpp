#include "internal/graph/runtime/optimizer/passes/MediaNodeFusionPass.h"

namespace media::ffmpeg::graph {

const char* MediaNodeFusionPass::name() const noexcept
{
    return "node-fusion";
}

::media::Status MediaNodeFusionPass::run(MediaGraph& graph,
                                          const MediaGraphOptimizationPolicy& policy,
                                          MediaGraphOptimizationReport& report)
{
    if (!policy.enableNodeFusion) {
        report.info(name(), "node fusion disabled");
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaNodeFusionPass is unsupported: structural fusion is not implemented"));
}

} // namespace media::ffmpeg::graph
