#include "internal/graph/preset/MediaPipelinePreset.h"

#include <utility>

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

::media::Result<MediaGraph> MediaPipelinePreset::createLocalFileTranscodeSkeleton(const MediaPipelinePresetOptions& options)
{
    if (options.inputUrl.empty() || options.outputUrl.empty()) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileTranscodeSkeleton requires inputUrl and outputUrl"));
    }

    MediaGraph graph;

    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "file-input");
    const MediaNodeId fileOutput = graph.addNode(MediaNodeKind::FileOutput, "file-output");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "demux");
    const MediaNodeId split = graph.addNode(MediaNodeKind::StreamSplit, "stream-split");
    const MediaNodeId videoDecode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    const MediaNodeId videoEncode = graph.addNode(MediaNodeKind::VideoEncode, "video-encode");
    const MediaNodeId audioCopy = graph.addNode(MediaNodeKind::AudioCopy, "audio-copy");
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
    graph.addInputPort(split, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(split, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addOutputPort(videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false, true);
    graph.addInputPort(videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false, true);
    graph.addOutputPort(videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(audioCopy, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addOutputPort(audioCopy, "packet", MediaStreamKind::Audio, MediaEdgeKind::CopiedPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    graph.addInputPort(mux, "packet", MediaStreamKind::Any, MediaEdgeKind::Unknown, MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", demux, "input", "input-format");
    graph.connect(demux, "packet", split, "packet", "demux-to-split");
    graph.connect(split, "video", videoDecode, "packet", "video-packet");
    graph.connect(videoDecode, "frame", videoEncode, "frame", "video-frame");
    graph.connect(videoEncode, "packet", mux, "packet", "video-encoded-packet");
    graph.connect(split, "audio", audioCopy, "packet", "audio-packet");
    graph.connect(audioCopy, "packet", mux, "packet", "audio-copy-packet");
    graph.connect(fileOutput, "format", mux, "format", "output-format");

    return ::media::Result<MediaGraph>::success(std::move(graph));
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
