#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {

class HardwareTransferNode final : public FFmpegNodeRuntime {
public:
    explicit HardwareTransferNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status transferOrForward(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status downloadHardwareFrame(MediaGraphExecutionContext& context,
                                          const MediaBufferRef& buffer,
                                          const AVFrame* sourceFrame);
};

} // namespace media::ffmpeg::graph
