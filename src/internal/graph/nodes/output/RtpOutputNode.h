#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRtpOutputSession.h"

namespace media::ffmpeg::graph {

class RtpOutputNode final : public FFmpegNodeRuntime {
public:
    explicit RtpOutputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status openIfNeeded(MediaGraphExecutionContext& context);

private:
    FFmpegRtpOutputSession m_session;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
