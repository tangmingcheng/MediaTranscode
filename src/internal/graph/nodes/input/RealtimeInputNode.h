#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

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
    ::media::Status openInput(MediaGraphExecutionContext& context);

private:
    ::media::ffmpeg::InputFormatContextPtr m_context;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
