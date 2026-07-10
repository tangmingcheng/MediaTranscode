#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <cstdint>
#include <deque>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class VideoFrameRateNode final : public FFmpegNodeRuntime {
public:
    explicit VideoFrameRateNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    friend struct VideoFrameRateNodeLifecycleTestAccess;

    ::media::Result<MediaNodeProcessResult> continueTerminal(MediaGraphExecutionContext& context);
    ::media::Status initializeFromFirstFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status drainPending(MediaGraphExecutionContext& context);
    ::media::Status queueFrameReference(const AVFrame* sourceFrame, int64_t outputPts);
    ::media::Status rememberLastInputFrame(const MediaBufferRef& buffer);

    int64_t targetPtsForIndex(int64_t outputIndex) const noexcept;
    int64_t targetFrameDuration() const noexcept;
    const AVFrame* chooseSourceFrameForTarget(const AVFrame* currentFrame,
                                              int64_t currentPts,
                                              int64_t targetPts) const noexcept;
    bool enabled() const noexcept;
    void resetRuntimeState() noexcept;

private:
    bool m_initialized = false;
    bool m_started = false;
    bool m_flushed = false;

    AVRational m_inputTimeBase { 0, 1 };
    AVRational m_targetFramePeriod { 0, 1 };

    int64_t m_startPts = 0;
    int64_t m_nextOutputIndex = 0;
    int64_t m_lastInputPts = 0;
    int64_t m_lastOutputPts = AV_NOPTS_VALUE;

    MediaBufferRef m_lastInputFrame;
    std::deque<MediaBufferRef> m_pendingFrames;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    MediaBufferRef m_terminalBuffer;
    bool m_terminalPending = false;
    bool m_terminalIsEof = false;
};

} // namespace media::ffmpeg::graph
