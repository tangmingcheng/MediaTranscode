#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

namespace media::ffmpeg::graph {

MediaControlBuffer::MediaControlBuffer(MediaControlBufferKind kind)
    : m_kind(kind)
{
    initialize(kind);
}

MediaControlBuffer::MediaControlBuffer(
    MediaControlBufferKind kind,
    std::uint64_t generation)
    : m_kind(kind)
    , m_generation(generation)
{
    initialize(kind);
}

void MediaControlBuffer::initialize(MediaControlBufferKind kind)
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

std::optional<std::uint64_t>
MediaControlBuffer::generation() const noexcept
{
    return m_generation;
}

} // namespace media::ffmpeg::graph
