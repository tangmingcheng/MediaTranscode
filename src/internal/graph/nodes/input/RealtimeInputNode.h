#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputSession.h"

namespace media::ffmpeg::graph {

class RealtimeInputNode final : public FFmpegNodeRuntime {
public:
    explicit RealtimeInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status openIfNeeded(MediaGraphExecutionContext& context);

private:
    FFmpegRealtimeInputSession m_session;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
