#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvSourceLifecycleMode : std::uint8_t {
    FailSessionOnSourceLoss = 0,
    PreserveActivatedOutput = 1
};

struct MediaAvSourceLifecyclePlan final {
    MediaAvSourceLifecycleMode mode;
};

} // namespace media::ffmpeg::graph
