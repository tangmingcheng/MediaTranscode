#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

namespace media::ffmpeg::graph {

class AudioResampleNode final : public FFmpegCodecNodeRuntime {
public:
    explicit AudioResampleNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindEncoderContext(MediaGraphExecutionContext& context);
    ::media::Status processFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status emitConvertedFrame(MediaGraphExecutionContext& context, const AVFrame* inputFrame, const MediaBufferRef& inputBuffer);
    ::media::Status ensureSwrInitialized(const AVFrame* inputFrame);
    bool frameMatchesEncoder(const AVFrame* frame) const noexcept;

private:
    ::media::ffmpeg::SwrContextPtr m_swr;
};

} // namespace media::ffmpeg::graph
