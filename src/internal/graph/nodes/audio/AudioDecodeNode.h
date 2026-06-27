#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

namespace media::ffmpeg::graph {

class AudioDecodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit AudioDecodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status receiveFrames(MediaGraphExecutionContext& context);
};

} // namespace media::ffmpeg::graph
