#pragma once

#include "internal/graph/runtime/runloop/MediaGraphRunLoop.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

struct MediaGraphExecutionResult {
    MediaGraphRunLoopResult runLoop;
    MediaGraphRuntimeReport report;
    bool started = false;
    bool stopped = false;
};

} // namespace media::ffmpeg::graph
