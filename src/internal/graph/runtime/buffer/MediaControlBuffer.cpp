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

std::optional<MediaControlBufferFlow>
MediaControlBuffer::flow() const noexcept
{
    switch (m_kind) {
    case MediaControlBufferKind::Eof:
        return isEof() && !isFlush()
            ? std::optional(MediaControlBufferFlow::Terminal)
            : std::nullopt;
    case MediaControlBufferKind::Flush:
        return isFlush() && !isEof()
            ? std::optional(MediaControlBufferFlow::Continue)
            : std::nullopt;
    case MediaControlBufferKind::Abort:
        return !isEof() && !isFlush()
            ? std::optional(MediaControlBufferFlow::Terminal)
            : std::nullopt;
    case MediaControlBufferKind::Unknown:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::uint64_t>
MediaControlBuffer::generation() const noexcept
{
    return m_generation;
}

::media::Result<MediaControlBufferClassification>
classifyMediaControlBuffer(const MediaBufferRef& buffer)
{
    using Result =
        ::media::Result<MediaControlBufferClassification>;
    const auto* control =
        dynamic_cast<const MediaControlBuffer*>(buffer.get());
    if (!buffer || !control ||
        buffer->type() != MediaBufferType::Control ||
        buffer->payloadKind() != MediaPayloadKind::ControlSignal ||
        buffer->streamKind() != MediaStreamKind::Control) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Control classification requires an exact control buffer"));
    }
    const auto flow = control->flow();
    if (!flow) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Control kind and buffer flags are inconsistent"));
    }
    return Result::success(MediaControlBufferClassification{
        control, control->controlKind(), *flow});
}

} // namespace media::ffmpeg::graph
