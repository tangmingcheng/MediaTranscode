#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class AudioStrategyNode final : public FFmpegNodeRuntime {
public:
    explicit AudioStrategyNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph
