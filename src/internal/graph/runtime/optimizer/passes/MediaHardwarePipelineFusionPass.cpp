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

    std::size_t hardwareAwareNodes = 0;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::HardwareTransfer ||
            node.kind == MediaNodeKind::VideoDecode ||
            node.kind == MediaNodeKind::VideoEncode ||
            node.kind == MediaNodeKind::VideoFilter) {
            ++hardwareAwareNodes;
        }
    }

    report.info(name(), "hardware-aware nodes detected=" + std::to_string(hardwareAwareNodes));
    report.info(name(), "fusion lowering deferred until concrete device capabilities are probed");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
