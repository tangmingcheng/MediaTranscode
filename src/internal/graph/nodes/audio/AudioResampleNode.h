#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <cstdint>

extern "C" {
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

class AudioResampleNode final : public FFmpegCodecNodeRuntime {
public:
    explicit AudioResampleNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindEncoderContext(MediaGraphExecutionContext& context);
    ::media::Status processFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status emitConvertedFrame(MediaGraphExecutionContext& context, const AVFrame* inputFrame, const MediaBufferRef& inputBuffer);
    ::media::Status ensureSwrInitialized(const AVFrame* inputFrame);
    bool frameMatchesEncoder(const AVFrame* frame) const noexcept;
    void resetRuntimeState() noexcept;

private:
    ::media::ffmpeg::SwrContextPtr m_swr;
    int64_t m_nextOutputPts = AV_NOPTS_VALUE;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
};

} // namespace media::ffmpeg::graph
