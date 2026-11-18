#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegFrameBuffer::FFmpegFrameBuffer(::media::ffmpeg::FramePtr frame, std::uint64_t logicalPayloadBytes)
    : m_frame(std::move(frame))
    , m_logicalPayloadBytes(logicalPayloadBytes)
{
    setPayloadKind(MediaPayloadKind::Frame);
    if (m_frame && (m_frame->flags & AV_FRAME_FLAG_KEY)) {
        addFlags(MediaBufferFlag::KeyFrame);
    }
}

MediaBufferType FFmpegFrameBuffer::type() const noexcept
{
    return MediaBufferType::Frame;
}

std::optional<std::uint64_t> FFmpegFrameBuffer::payloadFootprintBytes() const noexcept
{
    return m_frame ? std::optional<std::uint64_t>{m_logicalPayloadBytes} : std::nullopt;
}

AVFrame* FFmpegFrameBuffer::frame() noexcept
{
    return m_frame.get();
}

const AVFrame* FFmpegFrameBuffer::frame() const noexcept
{
    return m_frame.get();
}

MediaOwnedVideoFrame FFmpegFrameBuffer::takeFrame() noexcept
{
    return {std::move(m_frame), std::exchange(m_readiness, std::nullopt)};
}

::media::Status FFmpegFrameBuffer::setReadiness(MediaVideoFrameReadiness readiness)
{
    if (!m_frame || m_readiness)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Frame readiness is absent or already bound"));
    m_readiness.emplace(std::move(readiness));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
