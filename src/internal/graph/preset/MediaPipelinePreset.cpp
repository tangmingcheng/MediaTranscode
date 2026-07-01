#include "internal/graph/preset/MediaPipelinePreset.h"

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

namespace media::ffmpeg::graph {

::media::Result<MediaGraph> MediaPipelinePreset::create(MediaPipelinePresetKind kind,
                                                         const MediaPipelinePresetOptions& options)
{
    switch (kind) {
    case MediaPipelinePresetKind::LocalFileRemux:
        return createLocalFileRemux(options);
    case MediaPipelinePresetKind::LocalFileTranscodeSkeleton:
        return createLocalFileTranscodeSkeleton(options);
    case MediaPipelinePresetKind::RealtimeRtpSkeleton:
        return createRealtimeRtpSkeleton(options);
    default:
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("unsupported media pipeline preset"));
    }
}

::media::Result<MediaGraph> MediaPipelinePreset::createLocalFileRemux(const MediaPipelinePresetOptions&)
{
    return ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported("LocalFileRemux preset requires generic video packet copy and is not implemented"));
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
    builderOptions.kind = MediaRealtimeGraphKind::PacketRelay;
    builderOptions.inputUrl = options.inputUrl;
    builderOptions.outputUrl = options.outputUrl;
    builderOptions.includeAudio = options.includeAudio;
    builderOptions.includeVideo = options.includeVideo;
    builderOptions.enablePacketFanout = true;

    return MediaRealtimeGraphBuilder::buildPacketRelay(builderOptions);
}

} // namespace media::ffmpeg::graph
