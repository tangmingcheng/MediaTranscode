#pragma once

#include "internal/graph/runtime/optimizer/MediaGraphOptimizationPass.h"

namespace media::ffmpeg::graph {

class MediaHardwarePipelineFusionPass final : public MediaGraphOptimizationPass {
public:
    const char* name() const noexcept override;
    ::media::Status run(MediaGraph& graph,
                        const MediaGraphOptimizationPolicy& policy,
                        MediaGraphOptimizationReport& report) override;
};

} // namespace media::ffmpeg::graph
