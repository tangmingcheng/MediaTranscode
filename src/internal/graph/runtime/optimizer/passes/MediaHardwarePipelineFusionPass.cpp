#include "internal/graph/runtime/optimizer/passes/MediaHardwarePipelineFusionPass.h"

namespace media::ffmpeg::graph {

const char* MediaHardwarePipelineFusionPass::name() const noexcept
{
    return "hardware-pipeline-fusion";
}

::media::Status MediaHardwarePipelineFusionPass::run(MediaGraph& graph,
                                                      const MediaGraphOptimizationPolicy& policy,
                                                      MediaGraphOptimizationReport& report)
{
    if (!policy.zeroCopyPolicy.enabled()) {
        report.info(name(), "hardware fusion skipped: zero-copy disabled");
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaHardwarePipelineFusionPass is unsupported: hardware graph fusion is not implemented"));
}

} // namespace media::ffmpeg::graph
