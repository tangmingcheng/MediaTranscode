#include "internal/graph/preset/MediaPipelinePreset.h"

#include "internal/graph/builder/local/MediaLocalFileTranscodeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraph> MediaPipelinePreset::create(MediaPipelinePresetKind kind,
                                                         const MediaPipelinePresetOptions& options)
{
    switch (kind) {
    case MediaPipelinePresetKind::LocalFileRemux:
        return createLocalFileRemux(options);
    case MediaPipelinePresetKind::LocalFileTranscode:
        return createLocalFileTranscode(options);
    case MediaPipelinePresetKind::RealtimeRtpSkeleton:
        return createRealtimeRtpSkeleton(options);
    default:
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("unsupported media pipeline preset"));
    }
}

::media::Result<MediaGraph> MediaPipelinePreset::createLocalFileRemux(const MediaPipelinePresetOptions& options)
{
    if (options.inputUrl.empty() || options.outputUrl.empty()) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileRemux requires inputUrl and outputUrl"));
    }

    MediaGraph graph;

    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "file-input");
    const MediaNodeId fileOutput = graph.addNode(MediaNodeKind::FileOutput, "file-output");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "demux");
    const MediaNodeId mux = graph.addNode(MediaNodeKind::FileMux, "file-mux");

    graph.setNodeOption(fileInput, "url", options.inputUrl);
    graph.setNodeOption(fileOutput, "url", options.outputUrl);
    if (!options.outputFormat.empty()) {
        graph.setNodeOption(fileOutput, "format", options.outputFormat);
    }

    graph.addOutputPort(fileInput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    graph.addOutputPort(fileOutput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    graph.addInputPort(demux, "input", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    graph.addOutputPort(demux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    graph.addInputPort(mux, "packet", MediaStreamKind::Any, MediaEdgeKind::Unknown, MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", demux, "input", "input-format");
    graph.connect(fileOutput, "format", mux, "format", "output-format");
    graph.connect(demux, "packet", mux, "packet", "demux-packet");

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaGraph> MediaPipelinePreset::createLocalFileTranscode(const MediaPipelinePresetOptions& options)
{
    MediaLocalFileTranscodeGraphBuilderOptions builderOptions;
    builderOptions.inputUrl = options.inputUrl;
    builderOptions.outputUrl = options.outputUrl;
    builderOptions.outputFormat = options.outputFormat;
    builderOptions.includeAudio = options.includeAudio;
    builderOptions.includeVideo = options.includeVideo;

    return MediaLocalFileTranscodeGraphBuilder::build(builderOptions);
}

::media::Result<MediaGraph> MediaPipelinePreset::createRealtimeRtpSkeleton(const MediaPipelinePresetOptions&)
{
    MediaGraph graph;
    const MediaNodeId realtime = graph.addNode(MediaNodeKind::RealtimeInput, "realtime-input");
    const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "packet-fanout");
    const MediaNodeId rtpOutput = graph.addNode(MediaNodeKind::RtpOutput, "rtp-output");

    graph.addOutputPort(realtime, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(rtpOutput, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);

    graph.connect(realtime, "packet", fanout, "packet", "realtime-packet");
    graph.connect(fanout, "packet", rtpOutput, "packet", "rtp-output");

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
