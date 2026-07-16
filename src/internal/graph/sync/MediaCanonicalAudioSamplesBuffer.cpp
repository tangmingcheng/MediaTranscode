#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

namespace media::ffmpeg::graph {

MediaCanonicalAudioSamplesBuffer::MediaCanonicalAudioSamplesBuffer(
    MediaBufferRef media, std::vector<MediaAudioIntervalFragment> fragments)
    : m_media(std::move(media))
    , m_lineage(fragments.front().lineage)
    , m_interval({fragments.front().interval.begin,
                  fragments.back().interval.end,
                  fragments.front().interval.sampleRate})
    , m_fragments(std::move(fragments))
{
    setStreamKind(MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Frame);
}

::media::Result<MediaBufferRef> MediaCanonicalAudioSamplesBuffer::create(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage,
    MediaCanonicalAudioSampleInterval interval)
{
    return create(
        std::move(media),
        std::vector<MediaAudioIntervalFragment>{{std::move(lineage), interval}});
}

::media::Result<MediaBufferRef> MediaCanonicalAudioSamplesBuffer::create(
    MediaBufferRef media, std::vector<MediaAudioIntervalFragment> fragments)
{
    if (!media || media->type() != MediaBufferType::Frame ||
        media->streamKind() != MediaStreamKind::Audio || fragments.empty()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio samples require a valid frame and fragments"));
    }
    std::uint64_t generation = 0;
    int sampleRate = 0;
    std::int64_t expectedBegin = -1;
    for (const auto& fragment : fragments) {
        if (!fragment.lineage || fragment.interval.begin < 0 ||
            fragment.interval.end <= fragment.interval.begin ||
            fragment.interval.sampleRate <= 0) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical audio samples require valid fragments"));
        }
        if (auto valid = validateMediaCanonicalLineage(*fragment.lineage); !valid)
            return ::media::Result<MediaBufferRef>::failure(valid.error());
        if (generation == 0) {
            generation = fragment.lineage->generation;
            sampleRate = fragment.interval.sampleRate;
            expectedBegin = fragment.interval.begin;
        }
        if (fragment.lineage->generation != generation ||
            fragment.interval.sampleRate != sampleRate ||
            fragment.interval.begin != expectedBegin) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical audio fragments must be contiguous and same-generation"));
        }
        expectedBegin = fragment.interval.end;
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaCanonicalAudioSamplesBuffer(
            std::move(media), std::move(fragments))));
}

MediaBufferType MediaCanonicalAudioSamplesBuffer::type() const noexcept { return MediaBufferType::Event; }

} // namespace media::ffmpeg::graph
