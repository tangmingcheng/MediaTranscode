#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"

#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaBoundCanonicalAudioBuffer::create(
    MediaBufferRef media,
    MediaAudioPlaybackOrigin audioOrigin)
{
    auto canonical = std::dynamic_pointer_cast<MediaCanonicalAudioSamplesBuffer>(media);
    if (!canonical || !canonical->lineage() ||
        canonical->lineage()->generation != audioOrigin.generation ||
        audioOrigin.epochOutputSampleIndex < 0 || audioOrigin.outputSampleRate <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Bound canonical audio requires samples and matching active origin"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaBoundCanonicalAudioBuffer(std::move(canonical), audioOrigin)));
}

MediaBoundCanonicalAudioBuffer::MediaBoundCanonicalAudioBuffer(
    std::shared_ptr<MediaCanonicalAudioSamplesBuffer> media,
    MediaAudioPlaybackOrigin audioOrigin)
    : m_media(std::move(media)), m_audioOrigin(audioOrigin)
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Frame);
    setDiagnosticName("audio.bound_canonical");
}

MediaBufferType MediaBoundCanonicalAudioBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}
std::optional<std::uint64_t>
MediaBoundCanonicalAudioBuffer::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}
const std::shared_ptr<MediaCanonicalAudioSamplesBuffer>&
MediaBoundCanonicalAudioBuffer::media() const noexcept { return m_media; }
const MediaAudioPlaybackOrigin&
MediaBoundCanonicalAudioBuffer::audioOrigin() const noexcept { return m_audioOrigin; }

} // namespace media::ffmpeg::graph
