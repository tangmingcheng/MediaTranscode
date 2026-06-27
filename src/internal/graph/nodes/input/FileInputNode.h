#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/FFmpegRAII.h"

namespace media::ffmpeg::graph {

class FileInputNode final : public FFmpegNodeRuntime {
public:
    explicit FileInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status openInput(MediaGraphExecutionContext& context);

private:
    bool m_emitted = false;
    ::media::ffmpeg::InputFormatContextPtr m_context;
};

} // namespace media::ffmpeg::graph
