#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaEncodedAudioLineageBuffer::create(
    MediaBufferRef media,
    std::vector<MediaAudioIntervalFragment> fragments,
    MediaAudioPlaybackOrigin origin)
{
    if (!FFmpegPacketView::packet(media) || fragments.empty() ||
        origin.generation == 0 || origin.outputSampleRate <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Encoded audio lineage requires packet, fragments and active origin"));
    }
    std::int64_t expected = fragments.front().interval.begin;
    for (const auto& fragment : fragments) {
        if (!fragment.valid() ||
            !sameMediaCanonicalTimeline(*fragments.front().lineage, *fragment.lineage) ||
            fragment.lineage->generation != origin.generation ||
            fragment.interval.sampleRate != origin.outputSampleRate ||
            fragment.interval.begin != expected ||
            fragment.interval.end <= fragment.interval.begin) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Encoded audio lineage fragments must be exact and contiguous"));
        }
        expected = fragment.interval.end;
    }
    auto storage = MediaImmutableArray<MediaAudioIntervalFragment>::copy(fragments);
    if (!storage) return ::media::Result<MediaBufferRef>::failure(storage.error());
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaEncodedAudioLineageBuffer(
            std::move(media), std::move(storage).value(), origin)));
}

MediaEncodedAudioLineageBuffer::MediaEncodedAudioLineageBuffer(
    MediaBufferRef media,
    MediaImmutableArray<MediaAudioIntervalFragment> fragments,
    MediaAudioPlaybackOrigin origin)
    : m_media(std::move(media)), m_fragments(std::move(fragments)), m_origin(origin)
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Packet);
    setDiagnosticName("audio.encoded_lineage");
}
MediaBufferType MediaEncodedAudioLineageBuffer::type() const noexcept { return MediaBufferType::Event; }
std::optional<std::uint64_t> MediaEncodedAudioLineageBuffer::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}
const MediaBufferRef& MediaEncodedAudioLineageBuffer::media() const noexcept { return m_media; }
std::span<const MediaAudioIntervalFragment> MediaEncodedAudioLineageBuffer::fragments() const noexcept { return m_fragments.view(); }
const MediaAudioPlaybackOrigin& MediaEncodedAudioLineageBuffer::audioOrigin() const noexcept { return m_origin; }

} // namespace media::ffmpeg::graph
