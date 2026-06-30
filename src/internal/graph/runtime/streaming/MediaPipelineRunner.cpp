#include "internal/graph/runtime/streaming/MediaPipelineRunner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaPipelineRunResult> MediaPipelineRunner::runPreset(
    MediaPipelinePresetKind presetKind,
    const MediaPipelinePresetOptions& options)
{
    MediaStreamingSession session;
    auto prepareStatus = session.prepare(presetKind, options);
    if (!prepareStatus) {
        return ::media::Result<MediaPipelineRunResult>::failure(prepareStatus.error());
    }

    auto runResult = session.runtime().run();
    if (!runResult) {
        session.abort();
        return ::media::Result<MediaPipelineRunResult>::failure(runResult.error());
    }

    MediaPipelineRunResult result;
    result.run = runResult.value();
    result.report = session.report();
    return ::media::Result<MediaPipelineRunResult>::success(result);
}

} // namespace media::ffmpeg::graph
