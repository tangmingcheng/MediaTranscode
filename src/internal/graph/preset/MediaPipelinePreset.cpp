#include "internal/graph/preset/MediaPipelinePreset.h"

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraph> MediaPipelinePreset::create(MediaPipelinePresetKind kind,
                                                         const MediaPipelinePresetOptions& options)
{
    switch (kind) {
    case MediaPipelinePresetKind::LocalFileTranscodeSkeleton:
        return createLocalFileTranscodeSkeleton(options);
    case MediaPipelinePresetKind::RealtimeRtpSkeleton:
        return createRealtimeRtpSkeleton(options);
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
    builderOptions.parameters.execution.includeVideo = options.includeVideo;

    return LocalFileTranscodeGraphBuilder::build(builderOptions);
}

::media::Result<MediaGraph> MediaPipelinePreset::createRealtimeRtpSkeleton(const MediaPipelinePresetOptions& options)
{
    MediaRealtimeGraphBuilderOptions builderOptions;
    builderOptions.kind = MediaRealtimeGraphKind::RtpTranscode;
    builderOptions.inputUrl = options.inputUrl;
    builderOptions.outputUrl = options.outputUrl;
    builderOptions.input.url = options.inputUrl;
    builderOptions.output.host = options.outputUrl.empty() ? "127.0.0.1" : options.outputUrl;
    builderOptions.includeAudio = options.includeAudio;
    builderOptions.includeVideo = options.includeVideo;
    builderOptions.parameters.execution.includeAudio = options.includeAudio;
    builderOptions.parameters.execution.includeVideo = options.includeVideo;
    builderOptions.parameters.video.bFrames = 0;

    auto realtime = MediaRealtimeGraphBuilder::build(builderOptions);
    if (!realtime) {
        return ::media::Result<MediaGraph>::failure(realtime.error());
    }
    return ::media::Result<MediaGraph>::success(std::move(realtime).value().graph);
}

} // namespace media::ffmpeg::graph
