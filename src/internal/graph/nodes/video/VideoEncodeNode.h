#pragma once

#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

namespace media::ffmpeg::graph {

class VideoEncodeNode final : public FFmpegCodecNodeRuntime {
public:
    explicit VideoEncodeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& codecBuffer);
    ::media::Status receivePackets(MediaGraphExecutionContext& context);
    ::media::Status drainEncoderForStop();

private:
    bool m_encoderConfigEmitted = false;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
};

} // namespace media::ffmpeg::graph
