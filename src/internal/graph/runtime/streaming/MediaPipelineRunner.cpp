#include "internal/graph/runtime/streaming/MediaPipelineRunner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaPipelineRunResult> MediaPipelineRunner::runPresetUntilIdle(
    MediaPipelinePresetKind presetKind,
    const MediaPipelinePresetOptions& options,
    MediaGraphRunLoopConfig runLoopConfig)
{
    MediaStreamingSession session;
    auto prepareStatus = session.prepare(presetKind, options);
    if (!prepareStatus) {
        return ::media::Result<MediaPipelineRunResult>::failure(prepareStatus.error());
    }

    auto run = MediaGraphRunLoop::runUntilIdle(session.runtime(), runLoopConfig);
    if (!run) {
        session.abort();
        return ::media::Result<MediaPipelineRunResult>::failure(run.error());
    }

    MediaPipelineRunResult result;
    result.runLoop = run.value();
    result.report = session.report();
    return ::media::Result<MediaPipelineRunResult>::success(result);
}

} // namespace media::ffmpeg::graph
