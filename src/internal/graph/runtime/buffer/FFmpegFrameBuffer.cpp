#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegFrameBuffer::FFmpegFrameBuffer(::media::ffmpeg::FramePtr frame)
    : m_frame(std::move(frame))
{
    setPayloadKind(MediaPayloadKind::Frame);
    if (m_frame && m_frame->key_frame) {
        addFlags(MediaBufferFlag::KeyFrame);
    }
}

MediaBufferType FFmpegFrameBuffer::type() const noexcept
{
    return MediaBufferType::Frame;
}

AVFrame* FFmpegFrameBuffer::frame() noexcept
{
    return m_frame.get();
}

const AVFrame* FFmpegFrameBuffer::frame() const noexcept
{
    return m_frame.get();
}

::media::ffmpeg::FramePtr FFmpegFrameBuffer::takeFrame() noexcept
{
    return std::move(m_frame);
}

} // namespace media::ffmpeg::graph
