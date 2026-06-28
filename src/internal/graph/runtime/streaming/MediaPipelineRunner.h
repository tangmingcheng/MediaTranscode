#pragma once

#include "internal/graph/preset/MediaPipelinePreset.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/streaming/MediaStreamingSession.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaPipelineRunResult {
    MediaGraphRunResult run;
    MediaGraphRuntimeReport report;
};

class MediaPipelineRunner final {
public:
    static ::media::Result<MediaPipelineRunResult> runPreset(
        MediaPipelinePresetKind presetKind,
        const MediaPipelinePresetOptions& options);
};

} // namespace media::ffmpeg::graph
