#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class RtpOutputNode final : public FFmpegNodeRuntime {
public:
    explicit RtpOutputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph
