#pragma once

#include "internal/graph/preset/MediaPipelinePreset.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/runloop/MediaGraphRunLoop.h"
#include "internal/graph/runtime/streaming/MediaStreamingSession.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaPipelineRunResult {
    MediaGraphRunLoopResult runLoop;
    MediaGraphRuntimeReport report;
};

class MediaPipelineRunner final {
public:
    static ::media::Result<MediaPipelineRunResult> runPresetUntilIdle(
        MediaPipelinePresetKind presetKind,
        const MediaPipelinePresetOptions& options,
        MediaGraphRunLoopConfig runLoopConfig = {});
};

} // namespace media::ffmpeg::graph
