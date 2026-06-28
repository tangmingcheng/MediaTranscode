#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/compiler/MediaGraphInstruction.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphInstructionLowerer final {
public:
    static ::media::Result<MediaGraphInstructionPlan> lower(const MediaGraph& graph);

private:
    static MediaGraphInstructionKind classify(MediaNodeKind kind) noexcept;
};

} // namespace media::ffmpeg::graph
