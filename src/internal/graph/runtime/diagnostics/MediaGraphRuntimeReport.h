#pragma once

#include "internal/graph/runtime/backpressure/MediaBackpressureController.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaGraphRuntimeReport {
    MediaGraphRuntimeState state = MediaGraphRuntimeState::Empty;
    MediaGraphRuntimeMetrics metrics;
    MediaBackpressureReport backpressure;

    std::string summary() const;
};

class MediaGraphRuntimeReporter final {
public:
    static MediaGraphRuntimeReport capture(MediaGraphRuntime& runtime);
    static MediaGraphRuntimeReport capture(const MediaGraphRuntime& runtime);
};

} // namespace media::ffmpeg::graph
