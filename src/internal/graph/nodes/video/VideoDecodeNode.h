#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

namespace media::ffmpeg::graph {

class VideoDecodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoDecodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    friend struct VideoDecodeNodeLifecycleTestAccess;

    ::media::Result<bool> receiveFrames(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;

    MediaInputTerminalTracker m_terminals { { "packet" } };
    bool m_eofEmitted = false;
    bool m_receivePending = false;
    bool m_flushPending = false;
    bool m_flushIsEof = false;
    bool m_flushSent = false;
    MediaBufferRef m_flushBuffer;
};

} // namespace media::ffmpeg::graph
