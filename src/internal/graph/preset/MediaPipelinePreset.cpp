#include "internal/graph/preset/MediaPipelinePreset.h"

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

namespace media::ffmpeg::graph {

::media::Result<MediaGraph> MediaPipelinePreset::create(MediaPipelinePresetKind kind,
                                                         const MediaPipelinePresetOptions& options)
{
    switch (kind) {
    case MediaPipelinePresetKind::LocalFileTranscodeSkeleton:
        return createLocalFileTranscodeSkeleton(options);
    default:
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("unsupported media pipeline preset"));
    }
}

::media::Result<MediaGraph> MediaPipelinePreset::createLocalFileTranscodeSkeleton(const MediaPipelinePresetOptions& options)
{
    LocalFileTranscodeOptions builderOptions;
    builderOptions.inputUrl = options.inputUrl;
    builderOptions.outputUrl = options.outputUrl;
    builderOptions.outputFormat = options.outputFormat;
    builderOptions.parameters.execution.includeAudio = options.includeAudio;
    builderOptions.parameters.queues = options.queues;

    return LocalFileTranscodeGraphBuilder::build(builderOptions);
}

} // namespace media::ffmpeg::graph
