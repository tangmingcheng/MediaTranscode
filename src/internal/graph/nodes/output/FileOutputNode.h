#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class FileOutputNode final : public FFmpegNodeRuntime {
public:
    explicit FileOutputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph
