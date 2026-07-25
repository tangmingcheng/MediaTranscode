#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaPlaybackEpoch final {
    MediaRunningTime sourceStart;
    MediaRunningTime masterRelease;
    std::uint64_t generation;

    bool operator==(const MediaPlaybackEpoch&) const = default;
};

} // namespace media::ffmpeg::graph
