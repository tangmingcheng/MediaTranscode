#pragma once

#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/execution/MediaGraphExecutionMode.h"

namespace media::ffmpeg::graph {

struct MediaGraphExecutionOptions {
    MediaGraphExecutionMode mode = MediaGraphExecutionMode::SingleThreaded;
    MediaThreadingPolicy threadingPolicy;

    bool autoRegisterDefaultNodes = true;
    bool stopOnCompletion = true;
};

} // namespace media::ffmpeg::graph
