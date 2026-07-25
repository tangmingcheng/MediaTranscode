#pragma once

#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaCanonicalAudioSourceTimeline final {
public:
    static ::media::Result<MediaCanonicalAudioSourceTimeline> create(
        int sampleRate);

    ::media::Result<MediaCanonicalAudioSampleInterval> append(
        MediaRunningTime canonicalPresentation,
        std::uint32_t sampleCount,
        std::uint64_t generation,
        std::uint64_t sequence);

    void reset() noexcept;

private:
    explicit MediaCanonicalAudioSourceTimeline(int sampleRate) noexcept;

    int m_sampleRate;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastSequence = 0;
    std::int64_t m_expectedNextBegin = 0;
    bool m_initialized = false;
};

} // namespace media::ffmpeg::graph
