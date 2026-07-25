#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"

#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaDecodedAudioTrimInputBuffer::create(
    MediaBufferRef media,
    MediaAudioPlaybackOrigin audioOrigin,
    std::uint32_t trimLeadingSamples)
{
    auto canonical = std::dynamic_pointer_cast<MediaCanonicalAudioSamplesBuffer>(media);
    if (!canonical || !canonical->lineage() ||
        canonical->lineage()->generation != audioOrigin.generation ||
        audioOrigin.epochOutputSampleIndex < 0 || audioOrigin.outputSampleRate <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Decoded audio trim input requires canonical samples and matching origin"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaDecodedAudioTrimInputBuffer(
            std::move(canonical), audioOrigin, trimLeadingSamples)));
}

MediaDecodedAudioTrimInputBuffer::MediaDecodedAudioTrimInputBuffer(
    std::shared_ptr<MediaCanonicalAudioSamplesBuffer> media,
    MediaAudioPlaybackOrigin audioOrigin,
    std::uint32_t trimLeadingSamples)
    : m_media(std::move(media))
    , m_audioOrigin(audioOrigin)
    , m_trimLeadingSamples(trimLeadingSamples)
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Frame);
    setDiagnosticName("audio.decoded_trim_input");
}

MediaBufferType MediaDecodedAudioTrimInputBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

std::optional<std::uint64_t>
MediaDecodedAudioTrimInputBuffer::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}

const std::shared_ptr<MediaCanonicalAudioSamplesBuffer>&
MediaDecodedAudioTrimInputBuffer::media() const noexcept { return m_media; }
const MediaAudioPlaybackOrigin&
MediaDecodedAudioTrimInputBuffer::audioOrigin() const noexcept { return m_audioOrigin; }
std::uint32_t MediaDecodedAudioTrimInputBuffer::trimLeadingSamples() const noexcept
{
    return m_trimLeadingSamples;
}

} // namespace media::ffmpeg::graph
