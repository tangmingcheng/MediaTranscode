#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"

#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaAvReleasedAudioBuffer::create(
    MediaBufferRef media,
    std::uint32_t trimLeadingSamples)
{
    auto canonical = std::dynamic_pointer_cast<MediaCanonicalAccessUnitBuffer>(media);
    if (!canonical || canonical->stream() != MediaScheduledStream::Audio ||
        canonical->streamKind() != MediaStreamKind::Audio ||
        canonical->payloadKind() != MediaPayloadKind::Packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Released audio metadata requires a canonical audio packet"));
        }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaAvReleasedAudioBuffer(
            std::move(canonical), trimLeadingSamples)));
}

MediaAvReleasedAudioBuffer::MediaAvReleasedAudioBuffer(
    std::shared_ptr<MediaCanonicalAccessUnitBuffer> media,
    std::uint32_t trimLeadingSamples)
    : m_media(std::move(media))
    , m_trimLeadingSamples(trimLeadingSamples)
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Packet);
    setDiagnosticName("av_startup.released_audio");
}

MediaBufferType MediaAvReleasedAudioBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

std::optional<std::uint64_t>
MediaAvReleasedAudioBuffer::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}

const std::shared_ptr<MediaCanonicalAccessUnitBuffer>&
MediaAvReleasedAudioBuffer::media() const noexcept
{
    return m_media;
}

std::uint32_t MediaAvReleasedAudioBuffer::trimLeadingSamples() const noexcept
{
    return m_trimLeadingSamples;
}

} // namespace media::ffmpeg::graph
