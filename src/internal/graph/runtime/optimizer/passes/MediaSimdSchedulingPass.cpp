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
    if (policy.level == MediaGraphOptimizationLevel::Aggressive ||
        policy.level == MediaGraphOptimizationLevel::Realtime) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "MediaSimdSchedulingPass is unsupported: SIMD scheduling is not implemented"));
    }

    report.info(name(), "SIMD scheduling not requested");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
