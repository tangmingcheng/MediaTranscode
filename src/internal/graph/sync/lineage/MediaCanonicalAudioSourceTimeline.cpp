#include "internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.h"

#include "internal/graph/sync/MediaAudioSampleGrid.h"

#include <limits>

namespace media::ffmpeg::graph {

namespace {

::media::Result<std::int64_t> checkedIntervalEnd(
    std::int64_t begin,
    std::uint32_t sampleCount)
{
    const auto signedSampleCount = static_cast<std::int64_t>(sampleCount);
    if (begin > std::numeric_limits<std::int64_t>::max() - signedSampleCount) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio source interval is not representable"));
    }
    return ::media::Result<std::int64_t>::success(
        begin + signedSampleCount);
}

::media::Result<MediaCanonicalAudioSampleInterval> invalidTimeline(
    const char* message)
{
    return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaCanonicalAudioSourceTimeline::MediaCanonicalAudioSourceTimeline(
    int sampleRate) noexcept
    : m_sampleRate(sampleRate)
{
}

::media::Result<MediaCanonicalAudioSourceTimeline>
MediaCanonicalAudioSourceTimeline::create(int sampleRate)
{
    auto grid = MediaAudioSampleGrid::create(sampleRate);
    if (!grid) {
        return ::media::Result<MediaCanonicalAudioSourceTimeline>::failure(
            grid.error());
    }
    return ::media::Result<MediaCanonicalAudioSourceTimeline>::success(
        MediaCanonicalAudioSourceTimeline(sampleRate));
}

::media::Result<MediaCanonicalAudioSampleInterval>
MediaCanonicalAudioSourceTimeline::append(
    MediaRunningTime canonicalPresentation,
    std::uint32_t sampleCount,
    std::uint64_t generation,
    std::uint64_t sequence)
{
    if (generation == 0) {
        return invalidTimeline(
            "Canonical audio source timeline requires a non-zero generation");
    }
    if (sampleCount == 0) {
        return invalidTimeline(
            "Canonical audio source timeline requires a non-zero sample count");
    }
    if (sequence == 0) {
        return invalidTimeline(
            "Canonical audio source timeline requires a non-zero sequence");
    }

    std::int64_t begin = 0;
    if (!m_initialized) {
        auto grid = MediaAudioSampleGrid::create(m_sampleRate);
        if (!grid) {
            return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
                grid.error());
        }
        auto anchoredBegin = grid.value().nearestSample(canonicalPresentation);
        if (!anchoredBegin) {
            return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
                anchoredBegin.error());
        }
        begin = anchoredBegin.value();
    } else {
        if (generation != m_generation ||
            m_lastSequence == std::numeric_limits<std::uint64_t>::max() ||
            sequence != m_lastSequence + 1) {
            return invalidTimeline(
                "Canonical audio source timeline requires matching generation and contiguous sequence");
        }
        begin = m_expectedNextBegin;
    }

    auto end = checkedIntervalEnd(begin, sampleCount);
    if (!end) {
        return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
            end.error());
    }

    m_generation = generation;
    m_lastSequence = sequence;
    m_expectedNextBegin = end.value();
    m_initialized = true;
    return ::media::Result<MediaCanonicalAudioSampleInterval>::success(
        MediaCanonicalAudioSampleInterval{begin, end.value(), m_sampleRate});
}

void MediaCanonicalAudioSourceTimeline::reset() noexcept
{
    m_generation = 0;
    m_lastSequence = 0;
    m_expectedNextBegin = 0;
    m_initialized = false;
}

} // namespace media::ffmpeg::graph
