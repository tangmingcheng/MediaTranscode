#pragma once

#include "internal/graph/runtime/lifecycle/MediaRuntimeLifecycleStage.h"

#include <chrono>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRuntimeLifecycleEvent {
    MediaRuntimeLifecycleStage stage = MediaRuntimeLifecycleStage::Created;
    std::chrono::steady_clock::time_point timestamp;
    std::string message;
};

} // namespace media::ffmpeg::graph
