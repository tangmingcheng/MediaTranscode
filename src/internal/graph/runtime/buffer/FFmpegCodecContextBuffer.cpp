#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegCodecContextBuffer::FFmpegCodecContextBuffer(::media::ffmpeg::CodecContextPtr context)
    : m_ownership(FFmpegCodecContextOwnership::Owned)
    , m_context(std::move(context))
{
    setPayloadKind(MediaPayloadKind::CodecContext);
    if (m_context) {
        setStreamKind(FFmpegDescriptorMapper::toStreamKind(m_context->codec_type));
        setFormatDescriptor(FFmpegDescriptorMapper::fromCodecContext(m_context.get()));
    }
}

FFmpegCodecContextBuffer::FFmpegCodecContextBuffer(AVCodecContext* borrowedContext)
    : m_ownership(FFmpegCodecContextOwnership::Borrowed)
    , m_borrowedContext(borrowedContext)
{
    setPayloadKind(MediaPayloadKind::CodecContext);
    if (m_borrowedContext) {
        setStreamKind(FFmpegDescriptorMapper::toStreamKind(m_borrowedContext->codec_type));
        setFormatDescriptor(FFmpegDescriptorMapper::fromCodecContext(m_borrowedContext));
    }
}

MediaBufferType FFmpegCodecContextBuffer::type() const noexcept
{
    return MediaBufferType::CodecContext;
}

AVCodecContext* FFmpegCodecContextBuffer::context() noexcept
{
    return m_context ? m_context.get() : m_borrowedContext;
}

const AVCodecContext* FFmpegCodecContextBuffer::context() const noexcept
{
    return m_context ? m_context.get() : m_borrowedContext;
}

FFmpegCodecContextOwnership FFmpegCodecContextBuffer::ownership() const noexcept
{
    return m_ownership;
}

::media::ffmpeg::CodecContextPtr FFmpegCodecContextBuffer::takeContext() noexcept
{
    m_ownership = FFmpegCodecContextOwnership::Borrowed;
    m_borrowedContext = nullptr;
    return std::move(m_context);
}

} // namespace media::ffmpeg::graph
