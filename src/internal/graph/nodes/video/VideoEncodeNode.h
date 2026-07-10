#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

namespace media::ffmpeg::graph {

class VideoEncodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoEncodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& codecBuffer);
    ::media::Result<bool> receivePackets(MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> continueFlush(MediaGraphExecutionContext& context);
    ::media::Status drainEncoderForStop();
    void resetRuntimeState() noexcept;

private:
    bool m_encoderConfigEmitted = false;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    bool m_flushPending = false;
    bool m_receivePending = false;
    bool m_flushIsEof = false;
    bool m_flushSent = false;
    MediaBufferRef m_flushBuffer;
};

} // namespace media::ffmpeg::graph
