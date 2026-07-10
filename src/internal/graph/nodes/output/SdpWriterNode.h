#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <vector>

namespace media::ffmpeg::graph {

class SdpWriterNode final : public FFmpegNodeRuntime {
public:
    explicit SdpWriterNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status configureExpectedContexts(MediaGraphExecutionContext& context);
    ::media::Status writeIfReady(MediaGraphExecutionContext& context);

private:
    bool m_written = false;
    bool m_expectedContextsBound = false;
    int m_expectedContexts = 1;
    std::vector<MediaBufferRef> m_formatBuffers;
};

} // namespace media::ffmpeg::graph
