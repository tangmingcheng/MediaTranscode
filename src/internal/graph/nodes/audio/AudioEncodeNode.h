#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

namespace media::ffmpeg::graph {

class AudioEncodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit AudioEncodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status receivePackets(MediaGraphExecutionContext& context);
};

} // namespace media::ffmpeg::graph
