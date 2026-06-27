#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"

namespace media::ffmpeg::graph {

AVFrame* FFmpegFrameView::writableFrame(const MediaBufferRef& buffer) noexcept
{
    auto* frameBuffer = dynamic_cast<FFmpegFrameBuffer*>(buffer.get());
    return frameBuffer ? frameBuffer->frame() : nullptr;
}

const AVFrame* FFmpegFrameView::frame(const MediaBufferRef& buffer) noexcept
{
    const auto* frameBuffer = dynamic_cast<const FFmpegFrameBuffer*>(buffer.get());
    return frameBuffer ? frameBuffer->frame() : nullptr;
}

bool FFmpegFrameView::isFrame(const MediaBufferRef& buffer) noexcept
{
    return frame(buffer) != nullptr;
}

} // namespace media::ffmpeg::graph
