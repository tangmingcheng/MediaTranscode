#pragma once

#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaCanonicalAudioRealSource final {
    MediaCanonicalSourceStamp source;
    // Immutable provenance anchors. Resampling is not a one-to-one sample map.
    MediaCanonicalAudioSampleInterval sourceInterval;
    MediaCanonicalAudioSampleInterval mappedInterval;
};

struct MediaCanonicalAudioGeneratedSilence final {};

struct MediaCanonicalAudioContribution final {
    std::variant<MediaCanonicalAudioRealSource,
                 MediaCanonicalAudioGeneratedSilence> origin;
    MediaCanonicalAudioSampleInterval interval;
};

inline bool validMediaCanonicalAudioContribution(
    const MediaCanonicalAudioContribution& contribution) noexcept
{
    if (!contribution.interval.sampleCount()) return false;
    const auto* real = std::get_if<MediaCanonicalAudioRealSource>(&contribution.origin);
    if (!real) return true;
    return !real->source.identity.sourceIdentity.empty() &&
        real->source.identity.sourceSequence.value() != 0 &&
        real->source.generation != 0 && real->source.duration.nanoseconds() >= 0 &&
        real->sourceInterval.sampleCount() && real->mappedInterval.sampleCount() &&
        contribution.interval.sampleRate == real->mappedInterval.sampleRate &&
        contribution.interval.begin >= real->mappedInterval.begin &&
        contribution.interval.end <= real->mappedInterval.end;
}

} // namespace media::ffmpeg::graph
