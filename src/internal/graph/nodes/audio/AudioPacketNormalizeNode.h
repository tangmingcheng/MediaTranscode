#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class AudioPacketNormalizeNode final : public FFmpegNodeRuntime {
public:
    explicit AudioPacketNormalizeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph
