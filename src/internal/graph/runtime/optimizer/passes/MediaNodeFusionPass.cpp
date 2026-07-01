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

    std::size_t candidates = 0;
    for (const auto& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoTimestamp ||
            node.kind == MediaNodeKind::PacketNormalize ||
            node.kind == MediaNodeKind::VideoFrameRate) {
            ++candidates;
        }
    }

    report.info(name(), "fusion candidates detected=" + std::to_string(candidates));
    report.info(name(), "structural fusion is deferred until node capability metadata is complete");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
