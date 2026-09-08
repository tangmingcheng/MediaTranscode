#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

namespace media::ffmpeg::graph {

AVFrame* FFmpegFrameView::writableFrame(const MediaBufferRef& buffer) noexcept
{
    if (auto* canonical = dynamic_cast<MediaCanonicalVideoFrameBuffer*>(buffer.get()))
        return writableFrame(canonical->media());
    auto* frameBuffer = dynamic_cast<FFmpegFrameBuffer*>(buffer.get());
    return frameBuffer ? frameBuffer->frame() : nullptr;
}

const AVFrame* FFmpegFrameView::frame(const MediaBufferRef& buffer) noexcept
{
    if (const auto* canonical = dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(buffer.get()))
        return frame(canonical->media());
    const auto* frameBuffer = dynamic_cast<const FFmpegFrameBuffer*>(buffer.get());
    return frameBuffer ? frameBuffer->frame() : nullptr;
}

std::shared_ptr<const MediaCanonicalLineage>
FFmpegFrameView::canonicalLineage(const MediaBufferRef& buffer) noexcept
{
    const auto* canonical = dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(buffer.get());
    return canonical ? canonical->lineage() : nullptr;
}

const std::shared_ptr<MediaGraphPayloadCreditLease>&
FFmpegFrameView::payloadCredit(const MediaBufferRef& buffer) noexcept
{
    if (const auto* canonical =
            dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(buffer.get())) {
        return payloadCredit(canonical->media());
    }
    return buffer->payloadCredit();
}

bool FFmpegFrameView::isFrame(const MediaBufferRef& buffer) noexcept
{
    return frame(buffer) != nullptr;
}

} // namespace media::ffmpeg::graph
