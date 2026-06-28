#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeGraphBuilderResult> MediaRealtimeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    ::media::Result<MediaGraph> graphResult = ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported("unsupported realtime graph kind"));

    switch (options.kind) {
    case MediaRealtimeGraphKind::PacketRelay:
        graphResult = buildPacketRelay(options);
        break;
    case MediaRealtimeGraphKind::DecodeEncode:
        graphResult = buildDecodeEncode(options);
        break;
    case MediaRealtimeGraphKind::IngestToMux:
        graphResult = buildIngestToMux(options);
        break;
    }

    if (!graphResult) {
        return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(graphResult.error());
    }

    MediaRealtimeGraphBuilderResult result;
    result.graph = std::move(graphResult).value();
    return ::media::Result<MediaRealtimeGraphBuilderResult>::success(std::move(result));
}

::media::Result<MediaGraph> MediaRealtimeGraphBuilder::buildPacketRelay(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = realtimeEdgePolicy(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime-input");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "rtp-output");
    applyRealtimeInputOptions(graph, input, options);
    applyRealtimeOutputOptions(graph, output, options);

    graph.addOutputPort(input, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(output, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);

    if (options.enablePacketFanout) {
        const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "packet-fanout");
        graph.addInputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.addOutputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.connect(input, "packet", fanout, "packet", "realtime-input-packet", edgePolicy);
        graph.connect(fanout, "packet", output, "packet", "realtime-output-packet", edgePolicy);
    } else {
        graph.connect(input, "packet", output, "packet", "realtime-packet", edgePolicy);
    }

    if (options.enableSdpWriter) {
        const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter, "sdp-writer");
        graph.setNodeOption(sdp, "path", options.sdpPath);
        graph.addInputPort(sdp, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
        graph.connect(input, "packet", sdp, "packet", "sdp-packet-probe", edgePolicy, false);
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaGraph> MediaRealtimeGraphBuilder::buildDecodeEncode(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = realtimeEdgePolicy(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime-input");
    const MediaNodeId split = graph.addNode(MediaNodeKind::StreamSplit, "stream-split");
    const MediaNodeId videoDecode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    const MediaNodeId videoEncode = graph.addNode(MediaNodeKind::VideoEncode, "video-encode");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "rtp-output");

    applyRealtimeInputOptions(graph, input, options);
    applyRealtimeOutputOptions(graph, output, options);

    graph.addOutputPort(input, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(split, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(split, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addOutputPort(videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false, true);
    graph.addInputPort(videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false, true);
    graph.addOutputPort(videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(output, "packet", MediaStreamKind::Any, MediaEdgeKind::Unknown, MediaPayloadKind::Packet, true, true);

    graph.connect(input, "packet", split, "packet", "realtime-split-packet", edgePolicy);
    graph.connect(split, "video", videoDecode, "packet", "realtime-video-packet", edgePolicy, options.includeVideo);
    graph.connect(videoDecode, "frame", videoEncode, "frame", "realtime-video-frame", edgePolicy, options.includeVideo);
    graph.connect(videoEncode, "packet", output, "packet", "realtime-video-output", edgePolicy, options.includeVideo);

    if (options.includeAudio) {
        const MediaNodeId audioCopy = graph.addNode(MediaNodeKind::AudioCopy, "audio-copy");
        graph.addInputPort(audioCopy, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
        graph.addOutputPort(audioCopy, "packet", MediaStreamKind::Audio, MediaEdgeKind::CopiedPacket, MediaPayloadKind::Packet, false, true);
        graph.connect(split, "audio", audioCopy, "packet", "realtime-audio-packet", edgePolicy, false);
        graph.connect(audioCopy, "packet", output, "packet", "realtime-audio-output", edgePolicy, false);
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaGraph> MediaRealtimeGraphBuilder::buildIngestToMux(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = realtimeEdgePolicy(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime-input");
    const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "packet-fanout");
    const MediaNodeId mux = graph.addNode(options.enableRtpMux ? MediaNodeKind::RtpMux : MediaNodeKind::PacketMerge,
                                          options.enableRtpMux ? "rtp-mux" : "packet-merge");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "rtp-output");

    applyRealtimeInputOptions(graph, input, options);
    applyRealtimeOutputOptions(graph, output, options);

    graph.addOutputPort(input, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(mux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(mux, "packet", MediaStreamKind::Any, MediaEdgeKind::MuxedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(output, "packet", MediaStreamKind::Any, MediaEdgeKind::MuxedPacket, MediaPayloadKind::Packet, true, true);

    graph.connect(input, "packet", fanout, "packet", "realtime-ingest-packet", edgePolicy);
    graph.connect(fanout, "packet", mux, "packet", "realtime-mux-input", edgePolicy);
    graph.connect(mux, "packet", output, "packet", "realtime-mux-output", edgePolicy);

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

MediaEdgePolicy MediaRealtimeGraphBuilder::realtimeEdgePolicy(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::SpscRing;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;
    policy.queuePolicy.orderingPolicy = MediaQueueOrderingPolicy::Timestamp;
    policy.queuePolicy.capacity = options.queueCapacity > 0 ? options.queueCapacity : 8;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.collectMetrics = true;
    policy.queuePolicy.backpressurePolicy.mode = MediaBackpressureMode::Adaptive;
    policy.queuePolicy.backpressurePolicy.lowWatermark = policy.queuePolicy.capacity / 2;
    policy.queuePolicy.backpressurePolicy.highWatermark = options.highWatermark > 0 ? options.highWatermark : policy.queuePolicy.capacity - 2;
    policy.queuePolicy.backpressurePolicy.criticalWatermark = options.criticalWatermark > 0 ? options.criticalWatermark : policy.queuePolicy.capacity;
    policy.queuePolicy.backpressurePolicy.realtimePriority = true;
    policy.backpressurePolicy = policy.queuePolicy.backpressurePolicy;
    return policy;
}

void MediaRealtimeGraphBuilder::applyRealtimeInputOptions(MediaGraph& graph,
                                                          MediaNodeId nodeId,
                                                          const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.inputUrl.empty()) {
        graph.setNodeOption(nodeId, "url", options.inputUrl);
    }
    if (!options.mediaId.empty()) {
        graph.setNodeOption(nodeId, "media_id", options.mediaId);
    }
}

void MediaRealtimeGraphBuilder::applyRealtimeOutputOptions(MediaGraph& graph,
                                                           MediaNodeId nodeId,
                                                           const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.outputUrl.empty()) {
        graph.setNodeOption(nodeId, "url", options.outputUrl);
    }
    if (!options.mediaId.empty()) {
        graph.setNodeOption(nodeId, "media_id", options.mediaId);
    }
}

} // namespace media::ffmpeg::graph
