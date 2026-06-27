#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(::media::ffmpeg::InputFormatContextPtr context)
    : m_ownership(FFmpegFormatContextOwnership::Input)
    , m_inputContext(std::move(context))
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(::media::ffmpeg::OutputFormatContextPtr context)
    : m_ownership(FFmpegFormatContextOwnership::Output)
    , m_outputContext(std::move(context))
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(AVFormatContext* borrowedContext)
    : m_ownership(FFmpegFormatContextOwnership::Borrowed)
    , m_borrowedContext(borrowedContext)
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

MediaBufferType FFmpegFormatContextBuffer::type() const noexcept
{
    return MediaBufferType::FormatContext;
}

AVFormatContext* FFmpegFormatContextBuffer::context() noexcept
{
    if (m_inputContext) {
        return m_inputContext.get();
    }

    if (m_outputContext) {
        return m_outputContext.get();
    }

    return m_borrowedContext;
}

const AVFormatContext* FFmpegFormatContextBuffer::context() const noexcept
{
    if (m_inputContext) {
        return m_inputContext.get();
    }

    if (m_outputContext) {
        return m_outputContext.get();
    }

    return m_borrowedContext;
}

FFmpegFormatContextOwnership FFmpegFormatContextBuffer::ownership() const noexcept
{
    return m_ownership;
}

::media::ffmpeg::InputFormatContextPtr FFmpegFormatContextBuffer::takeInputContext() noexcept
{
    m_ownership = FFmpegFormatContextOwnership::Borrowed;
    return std::move(m_inputContext);
}

::media::ffmpeg::OutputFormatContextPtr FFmpegFormatContextBuffer::takeOutputContext() noexcept
{
    m_ownership = FFmpegFormatContextOwnership::Borrowed;
    return std::move(m_outputContext);
}

} // namespace media::ffmpeg::graph
