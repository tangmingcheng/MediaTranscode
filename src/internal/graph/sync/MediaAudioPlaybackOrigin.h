#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAudioPlaybackOrigin final {
    std::uint64_t generation;
    MediaRunningTime sourceStart;
    MediaRunningTime masterRelease;
    std::int64_t epochOutputSampleIndex;
    int outputSampleRate;

    friend bool operator==(const MediaAudioPlaybackOrigin&,
                           const MediaAudioPlaybackOrigin&) noexcept = default;
};

} // namespace media::ffmpeg::graph
