#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaCanonicalAudioSampleInterval final {
    std::int64_t begin;
    std::int64_t end;
    int sampleRate;
};

} // namespace media::ffmpeg::graph
