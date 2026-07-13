#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class RealtimeInputNode final : public FFmpegNodeRuntime {
public:
    RealtimeInputNode(MediaNodeId nodeId, MediaPreparedRealtimeInputKind expectedKind,
                      MediaPreparedRealtimeInput prepared);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    MediaPreparedRealtimeInputKind m_expectedKind;
    MediaPreparedRealtimeInput m_prepared;
    MediaBufferRef m_formatBuffer;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
