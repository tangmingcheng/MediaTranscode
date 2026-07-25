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

    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "MediaRedundantTransferEliminationPass is unsupported: graph rewrite is not implemented"));
}

} // namespace media::ffmpeg::graph
