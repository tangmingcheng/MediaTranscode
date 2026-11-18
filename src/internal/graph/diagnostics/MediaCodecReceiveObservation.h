#pragma once

#include <cstdint>
#include "internal/graph/diagnostics/MediaCodecTokenObservation.h"

namespace media::ffmpeg::graph {

// Wall-clock interval of the successful codec receive call producing this AU.
// Not a device execution interval or a bound used by scheduling.
struct MediaCodecReceiveObservation final {
    std::int64_t beginSteadyNs;
    std::int64_t endSteadyNs;
    std::optional<MediaCodecTokenObservation> token;
};

} // namespace media::ffmpeg::graph
