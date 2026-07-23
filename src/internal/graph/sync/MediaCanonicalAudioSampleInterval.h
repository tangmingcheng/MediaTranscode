#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaCanonicalAudioSampleInterval final {
    std::int64_t begin;
    std::int64_t end;
    int sampleRate;

    constexpr std::optional<std::int64_t> sampleCount() const noexcept
    {
        if (sampleRate <= 0 || end <= begin ||
            (begin < 0 && end > std::numeric_limits<std::int64_t>::max() + begin)) {
            return std::nullopt;
        }
        return end - begin;
    }
};

} // namespace media::ffmpeg::graph
