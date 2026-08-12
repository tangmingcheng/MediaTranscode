#include "internal/graph/runtime/buffer/MediaVideoOutputActivatedBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaVideoOutputActivatedBuffer::create(
    MediaProtocolOutputSessionKey sessionKey,
    MediaProtocolOutputActivation activation)
{
    if (!sessionKey.valid() || activation.generation == 0 ||
        activation.completedTransitionSequence) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly output activation requires one fixed-generation session"));
    }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaVideoOutputActivatedBuffer(
            std::move(sessionKey), activation)));
}

MediaVideoOutputActivatedBuffer::MediaVideoOutputActivatedBuffer(
    MediaProtocolOutputSessionKey sessionKey,
    MediaProtocolOutputActivation activation) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_activation(activation)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
}

MediaBufferType MediaVideoOutputActivatedBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaProtocolOutputSessionKey&
MediaVideoOutputActivatedBuffer::sessionKey() const noexcept
{
    return m_sessionKey;
}

const MediaProtocolOutputActivation&
MediaVideoOutputActivatedBuffer::activation() const noexcept
{
    return m_activation;
}

} // namespace media::ffmpeg::graph
