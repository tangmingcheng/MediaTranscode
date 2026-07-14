#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaAudioDriftServoPolicyValidator final {
public:
    static bool leadCoversWorstPositiveGap(std::int64_t leadNs,
                                           std::int64_t gapNs,
                                           int outputSampleRate,
                                           int maximumPositiveCorrectionPpm) noexcept;
};

} // namespace media::ffmpeg::graph
