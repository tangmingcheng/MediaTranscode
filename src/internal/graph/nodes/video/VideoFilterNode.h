#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <cstdint>
#include <set>
#include <string_view>

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
    VideoFilterNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaCodecLineageRegistry> lineageRegistry);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    friend struct VideoFilterNodeLifecycleTestAccess;

    ::media::Status bindEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status initializeGraph(MediaGraphExecutionContext& context, const MediaBufferRef& firstFrameBuffer);
    ::media::Status sendFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status submitPendingFrame(MediaGraphExecutionContext& context);
    ::media::Status attachPendingLineage();
    ::media::Status flushGraph(MediaGraphExecutionContext& context);
    ::media::Status drainFrames(MediaGraphExecutionContext& context, bool* produced = nullptr);
    ::media::Status emitFrame(MediaGraphExecutionContext& context, ::media::ffmpeg::FramePtr frame);
    ::media::Result<MediaNodeProcessResult> continueTerminal(MediaGraphExecutionContext& context);
    ::media::Status rescaleAndValidateFrame(AVFrame* frame) noexcept;
    void resetFilterGraph() noexcept;
    void resetRuntimeState() noexcept;

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
    bool m_filterEof = false;
    ::media::ffmpeg::FramePtr m_pendingFrame;
    std::shared_ptr<const MediaCanonicalLineage> m_pendingLineage;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    MediaBufferRef m_terminalBuffer;
    bool m_terminalPending = false;
    bool m_terminalIsEof = false;
    std::shared_ptr<MediaCodecLineageRegistry> m_lineageRegistry;
    std::set<std::uint64_t> m_lineageGenerations;
};

} // namespace media::ffmpeg::graph
