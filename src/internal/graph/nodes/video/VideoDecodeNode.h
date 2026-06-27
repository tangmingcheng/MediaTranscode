#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

namespace media::ffmpeg::graph {

class VideoDecodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoDecodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status receiveFrames(MediaGraphExecutionContext& context);
};

} // namespace media::ffmpeg::graph
