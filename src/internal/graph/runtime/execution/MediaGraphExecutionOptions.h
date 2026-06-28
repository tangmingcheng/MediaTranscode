#pragma once

#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/runloop/MediaGraphRunLoop.h"
#include "internal/graph/runtime/execution/MediaGraphExecutionMode.h"

namespace media::ffmpeg::graph {

struct MediaGraphExecutionOptions {
    MediaGraphExecutionMode mode = MediaGraphExecutionMode::SingleThreadedRunLoop;
    MediaThreadingPolicy threadingPolicy;
    MediaGraphRunLoopConfig runLoopConfig;

    bool autoRegisterDefaultNodes = true;
    bool stopOnRunLoopCompletion = true;
};

} // namespace media::ffmpeg::graph
