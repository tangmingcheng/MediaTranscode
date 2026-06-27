#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphOptimizationPolicy.h"
#include "internal/graph/runtime/optimizer/MediaGraphOptimizationReport.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphOptimizationPass {
public:
    virtual ~MediaGraphOptimizationPass() = default;

    MediaGraphOptimizationPass(const MediaGraphOptimizationPass&) = delete;
    MediaGraphOptimizationPass& operator=(const MediaGraphOptimizationPass&) = delete;

    virtual const char* name() const noexcept = 0;
    virtual ::media::Status run(MediaGraph& graph,
                                const MediaGraphOptimizationPolicy& policy,
                                MediaGraphOptimizationReport& report) = 0;

protected:
    MediaGraphOptimizationPass() = default;
};

class MediaGraphNoopOptimizationPass final : public MediaGraphOptimizationPass {
public:
    const char* name() const noexcept override;
    ::media::Status run(MediaGraph& graph,
                        const MediaGraphOptimizationPolicy& policy,
                        MediaGraphOptimizationReport& report) override;
};

} // namespace media::ffmpeg::graph
