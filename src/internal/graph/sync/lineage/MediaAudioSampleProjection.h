#pragma once

#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaAudioSampleProjection final {
public:
    static ::media::Result<MediaAudioSampleProjection> create(
        std::int64_t outputStart, int sourceSampleRate, int outputSampleRate);

    ::media::Result<MediaCanonicalAudioSampleInterval> append(
        std::int64_t sourceSamples);
    ::media::Result<MediaCanonicalAudioSampleInterval> extend(
        std::int64_t outputSamples);

    std::int64_t sourceSamples() const noexcept;
    std::int64_t outputEnd() const noexcept;
    int sourceSampleRate() const noexcept;
    int outputSampleRate() const noexcept;

private:
    MediaAudioSampleProjection(std::int64_t outputStart,
                               int sourceSampleRate,
                               int outputSampleRate) noexcept;

    std::int64_t m_outputStart = 0;
    std::int64_t m_sourceSamples = 0;
    std::int64_t m_outputEnd = 0;
    int m_sourceSampleRate = 0;
    int m_outputSampleRate = 0;
};

} // namespace media::ffmpeg::graph
