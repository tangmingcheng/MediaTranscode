#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
// A media-cadence contract, not a CPU/driver wall-clock execution bound.
struct MediaPreparedVideoRandomAccessEnvelope final {
    std::uint64_t maximumIdrDistanceFrames;
    MediaRational outputCadence;
    bool firstSubmittedFrameIsIdr;
    std::string authority;
    friend bool operator==(const MediaPreparedVideoRandomAccessEnvelope&,
                           const MediaPreparedVideoRandomAccessEnvelope&) = default;
};
} // namespace media::ffmpeg::graph
