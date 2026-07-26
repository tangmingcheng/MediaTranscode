#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaPlaybackEpochActivatedBuffer::create(
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin,
    std::optional<std::uint64_t> completedTransitionSequence)
{
    if (!groupKey.valid() || epoch.generation == 0 ||
        audioOrigin.generation != epoch.generation ||
        audioOrigin.sourceStart != epoch.sourceStart ||
        audioOrigin.masterRelease != epoch.masterRelease ||
        audioOrigin.epochOutputSampleIndex < 0 ||
        audioOrigin.outputSampleRate <= 0 ||
        (completedTransitionSequence &&
         *completedTransitionSequence == 0)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch activation event is incomplete"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaPlaybackEpochActivatedBuffer(
            std::move(groupKey), epoch, audioOrigin,
            completedTransitionSequence)));
}

MediaPlaybackEpochActivatedBuffer::MediaPlaybackEpochActivatedBuffer(
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin,
    std::optional<std::uint64_t> completedTransitionSequence)
    : m_groupKey(std::move(groupKey))
    , m_epoch(epoch)
    , m_audioOrigin(audioOrigin)
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
    return m_epoch;
}

const MediaAudioPlaybackOrigin&
MediaPlaybackEpochActivatedBuffer::audioOrigin() const noexcept
{
    return m_audioOrigin;
}

std::optional<std::uint64_t>
MediaPlaybackEpochActivatedBuffer::completedTransitionSequence() const noexcept
{
    return m_completedTransitionSequence;
}

} // namespace media::ffmpeg::graph
