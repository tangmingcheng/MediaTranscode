#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

struct MediaGraphExecutionResult {
    MediaGraphRunResult run;
    MediaGraphRuntimeReport report;
    bool started = false;
    bool stopped = false;
};

} // namespace media::ffmpeg::graph
