#include "internal/graph/runtime/optimizer/passes/MediaSimdSchedulingPass.h"

namespace media::ffmpeg::graph {

const char* MediaSimdSchedulingPass::name() const noexcept
{
    return "simd-scheduling";
}

::media::Status MediaSimdSchedulingPass::run(MediaGraph& graph,
                                              const MediaGraphOptimizationPolicy& policy,
                                              MediaGraphOptimizationReport& report)
{
    std::size_t frameTransformNodes = 0;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoFilter ||
            node.kind == MediaNodeKind::VideoFrameRate ||
            node.kind == MediaNodeKind::AudioResample) {
            ++frameTransformNodes;
        }
    }

    if (policy.level == MediaGraphOptimizationLevel::Aggressive ||
        policy.level == MediaGraphOptimizationLevel::Realtime) {
        report.info(name(), "SIMD-aware scheduling candidates=" + std::to_string(frameTransformNodes));
    } else {
        report.info(name(), "SIMD scheduling analysis skipped for safe optimization level");
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
