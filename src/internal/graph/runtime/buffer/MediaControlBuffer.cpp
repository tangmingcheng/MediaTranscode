#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

namespace media::ffmpeg::graph {

MediaControlBuffer::MediaControlBuffer(MediaControlBufferKind kind)
    : m_kind(kind)
{
    setPayloadKind(MediaPayloadKind::ControlSignal);
    setStreamKind(MediaStreamKind::Control);

    if (kind == MediaControlBufferKind::Eof) {
        addFlags(MediaBufferFlag::Eof);
    } else if (kind == MediaControlBufferKind::Flush) {
        addFlags(MediaBufferFlag::Flush);
    }
}

MediaBufferType MediaControlBuffer::type() const noexcept
{
    return MediaBufferType::Control;
}

MediaControlBufferKind MediaControlBuffer::controlKind() const noexcept
{
    return m_kind;
}

} // namespace media::ffmpeg::graph
