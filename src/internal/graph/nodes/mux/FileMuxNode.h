#pragma once

#include "internal/graph/nodes/mux/FFmpegMuxWriter.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class FileMuxNode final : public FFmpegNodeRuntime {
public:
    explicit FileMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindMuxExpectations(MediaGraphExecutionContext& context);
    void releaseRuntimeViews() noexcept;
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    FFmpegMuxWriter m_writer;
    bool m_expectationsBound = false;
};

} // namespace media::ffmpeg::graph
