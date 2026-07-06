#pragma once

#include "internal/FFmpegRAII.h"
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

private:
    ::media::Status openOutput(MediaGraphExecutionContext& context);

private:
    ::media::ffmpeg::OutputFormatContextPtr m_context;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
