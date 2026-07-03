#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeGraphBuilder";

} // namespace

::media::Result<MediaRealtimeGraphBuilderResult> MediaRealtimeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    ::media::Result<MediaGraph> graphResult = ::media::Result<MediaGraph>::failure(
        ::media::ErrorInfo::unsupported("unsupported realtime graph kind"));

    switch (options.kind) {
    case MediaRealtimeGraphKind::PacketRelay:
        graphResult = buildPacketRelay(options);
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

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime.input", "Realtime packet input");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "realtime.rtp.output", "Realtime RTP output");

    if (auto status = applyRealtimeInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = applyRealtimeOutputOptions(graph, output, options); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, input, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, output, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (options.enablePacketFanout) {
        const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "realtime.packet.fanout", "Realtime packet fanout");
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, input, "packet", fanout, "packet", "realtime.input.packet -> realtime.packet.fanout.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, fanout, "packet", output, "packet", "realtime.packet.fanout.packet -> realtime.rtp.output.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
    } else {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, input, "packet", output, "packet", "realtime.input.packet -> realtime.rtp.output.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
    }

    if (options.enableSdpWriter) {
        const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter, "realtime.sdp.writer", "Realtime SDP writer");
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, sdp, "path", options.sdpPath); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, sdp, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, input, "packet", sdp, "packet", "realtime.input.packet -> realtime.sdp.writer.packet", edgePolicy, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaGraph> MediaRealtimeGraphBuilder::buildIngestToMux(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = realtimeEdgePolicy(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime.input", "Realtime packet input");
    const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "realtime.packet.fanout", "Realtime packet fanout");
    const MediaNodeId mux = graph.addNode(options.enableRtpMux ? MediaNodeKind::RtpMux : MediaNodeKind::PacketMerge,
                                          options.enableRtpMux ? "realtime.rtp.mux" : "realtime.packet.merge",
                                          options.enableRtpMux ? "Realtime RTP mux" : "Realtime packet merge");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "realtime.rtp.output", "Realtime RTP output");

    if (auto status = applyRealtimeInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = applyRealtimeOutputOptions(graph, output, options); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, input, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, fanout, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, mux, "packet", MediaStreamKind::Any, MediaEdgeKind::MuxedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, output, "packet", MediaStreamKind::Any, MediaEdgeKind::MuxedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, input, "packet", fanout, "packet", "realtime.input.packet -> realtime.packet.fanout.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, fanout, "packet", mux, "packet", "realtime.packet.fanout.packet -> realtime.mux.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, mux, "packet", output, "packet", "realtime.mux.packet -> realtime.rtp.output.packet", edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());

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

::media::Result<void> MediaRealtimeGraphBuilder::applyRealtimeInputOptions(MediaGraph& graph,
                                                                           MediaNodeId nodeId,
                                                                           const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.inputUrl.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "url", options.inputUrl); !status) return status;
    }
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeGraphBuilder::applyRealtimeOutputOptions(MediaGraph& graph,
                                                                            MediaNodeId nodeId,
                                                                            const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.outputUrl.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "url", options.outputUrl); !status) return status;
    }
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
