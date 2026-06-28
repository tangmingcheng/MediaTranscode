#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class FileOutputNode final : public FFmpegNodeRuntime {
public:
    explicit FileOutputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status openOutput(MediaGraphExecutionContext& context);

private:
    bool m_emitted = false;
    ::media::ffmpeg::OutputFormatContextPtr m_context;
};

} // namespace media::ffmpeg::graph
