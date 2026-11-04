#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

namespace media::ffmpeg::graph {

AVFrame* FFmpegFrameView::writableFrame(const MediaBufferRef& buffer) noexcept
{
    if (auto* source = dynamic_cast<MediaVideoDecodeBuffer*>(buffer.get()))
        return writableFrame(source->media());
    if (auto* canonical = dynamic_cast<MediaCanonicalVideoFrameBuffer*>(buffer.get()))
        return writableFrame(canonical->media());
    auto* frameBuffer = dynamic_cast<FFmpegFrameBuffer*>(buffer.get());
    return frameBuffer ? frameBuffer->frame() : nullptr;
}

const AVFrame* FFmpegFrameView::frame(const MediaBufferRef& buffer) noexcept
{
    if (const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get()))
        return frame(source->media());
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

std::shared_ptr<const MediaVideoDisplayLineage>
FFmpegFrameView::displayLineage(const MediaBufferRef& buffer) noexcept
{
    const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get());
    return source ? source->displayLineage() : nullptr;
}

const std::shared_ptr<MediaGraphPayloadCreditLease>&
FFmpegFrameView::payloadCredit(const MediaBufferRef& buffer) noexcept
{
    if (const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get()))
        return payloadCredit(source->media());
    if (const auto* canonical =
            dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(buffer.get())) {
        return payloadCredit(canonical->media());
    }
    return buffer->payloadCredit();
}

const MediaVideoFrameReadiness* FFmpegFrameView::readiness(const MediaBufferRef& buffer) noexcept
{
    if (const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get())) return readiness(source->media());
    if (const auto* canonical = dynamic_cast<const MediaCanonicalVideoFrameBuffer*>(buffer.get())) return readiness(canonical->media());
    const auto* frame = dynamic_cast<const FFmpegFrameBuffer*>(buffer.get());
    return frame ? frame->readiness() : nullptr;
}

bool FFmpegFrameView::isFrame(const MediaBufferRef& buffer) noexcept
{
    return frame(buffer) != nullptr;
}

} // namespace media::ffmpeg::graph
