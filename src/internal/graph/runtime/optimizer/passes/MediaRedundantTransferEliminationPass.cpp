#include "internal/graph/runtime/optimizer/passes/MediaRedundantTransferEliminationPass.h"

namespace media::ffmpeg::graph {

const char* MediaRedundantTransferEliminationPass::name() const noexcept
{
    return "redundant-transfer-elimination";
}

::media::Status MediaRedundantTransferEliminationPass::run(MediaGraph& graph,
                                                            const MediaGraphOptimizationPolicy& policy,
                                                            MediaGraphOptimizationReport& report)
{
    if (!policy.enableRedundantTransferElimination) {
        report.info(name(), "redundant transfer elimination disabled");
        return ::media::Status::success();
    }

    std::size_t hardwareTransferNodes = 0;
    for (const auto& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::HardwareTransfer) {
            ++hardwareTransferNodes;
        }
    }

    report.info(name(), "hardware transfer nodes detected=" + std::to_string(hardwareTransferNodes));
    report.info(name(), "rewrite deferred until zero-copy capability probing is connected");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
