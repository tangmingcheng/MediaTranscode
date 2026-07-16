#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaStartupReleaseTransactionBuffer::create(
    MediaBufferRef release)
{
    const auto* typed = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        release.get());
    if (!typed || !typed->groupKey().valid() ||
        typed->epoch().generation == 0 ||
        typed->audioOrigin().generation != typed->epoch().generation ||
        typed->audioOrigin().sourceStart != typed->epoch().sourceStart ||
        typed->audioOrigin().masterRelease != typed->epoch().masterRelease) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Startup release transaction requires a complete typed release"));
    }
    if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
            typed->releaseKind()); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaStartupReleaseTransactionBuffer(std::move(release))));
}

MediaStartupReleaseTransactionBuffer::MediaStartupReleaseTransactionBuffer(
    MediaBufferRef release)
    : m_release(std::move(release))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_sync.startup_release_transaction");
}

MediaBufferType MediaStartupReleaseTransactionBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaBufferRef&
MediaStartupReleaseTransactionBuffer::release() const noexcept
{
    return m_release;
}

const MediaAvSyncGroupKey&
MediaStartupReleaseTransactionBuffer::groupKey() const noexcept
{
    return typedRelease().groupKey();
}

MediaAvStartupReleaseKind
MediaStartupReleaseTransactionBuffer::releaseKind() const noexcept
{
    return typedRelease().releaseKind();
}

const MediaPlaybackEpoch&
MediaStartupReleaseTransactionBuffer::epoch() const noexcept
{
    return typedRelease().epoch();
}

const MediaAudioPlaybackOrigin&
MediaStartupReleaseTransactionBuffer::audioOrigin() const noexcept
{
    return typedRelease().audioOrigin();
}

const MediaAvStartupReleaseBuffer&
MediaStartupReleaseTransactionBuffer::typedRelease() const noexcept
{
    return static_cast<const MediaAvStartupReleaseBuffer&>(*m_release);
}

} // namespace media::ffmpeg::graph
