#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

namespace media::ffmpeg::graph {

MediaCanonicalAudioSamplesBuffer::MediaCanonicalAudioSamplesBuffer(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage,
    MediaCanonicalAudioSampleInterval interval)
    : m_media(std::move(media)), m_lineage(std::move(lineage)), m_interval(interval)
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Frame);
}

::media::Result<MediaBufferRef> MediaCanonicalAudioSamplesBuffer::create(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage,
    MediaCanonicalAudioSampleInterval interval)
{
    if (!media || media->type() != MediaBufferType::Frame ||
        media->streamKind() != MediaStreamKind::Audio || !lineage ||
        interval.begin < 0 || interval.end <= interval.begin || interval.sampleRate <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio samples require a valid frame, lineage and sample interval"));
    }
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaCanonicalAudioSamplesBuffer(std::move(media), std::move(lineage), interval)));
}

MediaBufferType MediaCanonicalAudioSamplesBuffer::type() const noexcept { return MediaBufferType::Event; }

} // namespace media::ffmpeg::graph
