#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {

class HardwareTransferNode final : public FFmpegNodeRuntime {
public:
    explicit HardwareTransferNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status transferOrForward(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status downloadHardwareFrame(MediaGraphExecutionContext& context,
                                          const MediaBufferRef& buffer,
                                          const AVFrame* sourceFrame);

    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
};

} // namespace media::ffmpeg::graph
