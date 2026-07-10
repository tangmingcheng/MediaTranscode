#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class DebugDumpNode final : public FFmpegNodeRuntime {
public:
    explicit DebugDumpNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph
