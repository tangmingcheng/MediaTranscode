#pragma once
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/session/MediaPreparedRtpControlEndpoint.h"
namespace media::ffmpeg::graph {
class MediaRtpControlOutputNode final : public FFmpegNodeRuntime {
public:
    explicit MediaRtpControlOutputNode(MediaNodeId node);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status bindEndpoint(MediaPreparedRtpControlEndpoint endpoint);
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
private:
    std::optional<MediaPreparedRtpControlEndpoint> m_endpoint;
    bool m_started = false;
};
} // namespace media::ffmpeg::graph
