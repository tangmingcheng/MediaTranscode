#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class VideoFilterNode final : public FFmpegNodeRuntime {
public:
    explicit VideoFilterNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status initializeGraph(MediaGraphExecutionContext& context, const MediaBufferRef& firstFrameBuffer);
    ::media::Status sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status flushGraph(MediaGraphExecutionContext& context);
    ::media::Status drainFrames(MediaGraphExecutionContext& context);
    ::media::Status emitFrame(MediaGraphExecutionContext& context, ::media::ffmpeg::FramePtr frame);
    ::media::Status rescaleAndValidateFrame(AVFrame* frame) noexcept;
    void resetFilterGraph() noexcept;

private:
    MediaBufferRef m_encoderConfig;
    AVCodecContext* m_encoderContext = nullptr;
    ::media::ffmpeg::FilterGraphPtr m_filterGraph;
    AVFilterContext* m_bufferSrcContext = nullptr;
    AVFilterContext* m_bufferSinkContext = nullptr;
    AVRational m_inputTimeBase { 0, 1 };
    AVRational m_sinkTimeBase { 0, 1 };
    int64_t m_lastSubmittedPts = AV_NOPTS_VALUE;
    bool m_graphInitialized = false;
    bool m_flushed = false;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
};

} // namespace media::ffmpeg::graph
