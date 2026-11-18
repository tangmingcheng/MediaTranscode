#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaPlaybackEpochActivatedBuffer::create(
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackActivation activation,
    std::optional<std::uint64_t> completedTransitionSequence)
{
    if (!groupKey.valid() ||
        (completedTransitionSequence &&
         *completedTransitionSequence == 0)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch activation event is incomplete"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaPlaybackEpochActivatedBuffer(
            std::move(groupKey), std::move(activation),
            completedTransitionSequence)));
}

MediaPlaybackEpochActivatedBuffer::MediaPlaybackEpochActivatedBuffer(
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackActivation activation,
    std::optional<std::uint64_t> completedTransitionSequence)
    : m_groupKey(std::move(groupKey))
    , m_activation(std::move(activation))
    , m_completedTransitionSequence(completedTransitionSequence)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_sync.playback_epoch_activated");
}

MediaBufferType MediaPlaybackEpochActivatedBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaAvSyncGroupKey&
MediaPlaybackEpochActivatedBuffer::groupKey() const noexcept
{
    return m_groupKey;
}

const MediaPlaybackEpoch&
MediaPlaybackEpochActivatedBuffer::epoch() const noexcept
{
    return m_activation.epoch();
}

const MediaPlaybackActivation&
MediaPlaybackEpochActivatedBuffer::activation() const noexcept
{
    return m_activation;
}

std::optional<std::uint64_t>
MediaPlaybackEpochActivatedBuffer::completedTransitionSequence() const noexcept
{
    return m_completedTransitionSequence;
}

} // namespace media::ffmpeg::graph
