#pragma once

#include "internal/graph/nodes/mux/FFmpegMuxWriter.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class RtpMuxNode final : public FFmpegNodeRuntime {
public:
    explicit RtpMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status configureExpectations(MediaGraphExecutionContext& context);
    ::media::Status emitFormatIfReady(MediaGraphExecutionContext& context);
    void releaseRuntimeViews() noexcept;

private:
    FFmpegMuxWriter m_writer;
    bool m_expectationsBound = false;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
