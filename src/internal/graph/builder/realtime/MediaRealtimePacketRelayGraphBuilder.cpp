#include "internal/graph/builder/realtime/MediaRealtimePacketRelayGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeEdgePolicy.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimePacketRelayGraphBuilder";

} // namespace

::media::Result<MediaGraph> MediaRealtimePacketRelayGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = MediaRealtimeEdgePolicy::make(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime.input", "Realtime packet input");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "realtime.rtp.output", "Realtime RTP output");

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, output, options); !status) return ::media::Result<MediaGraph>::failure(status.error());

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
        if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, sdp, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, input, "packet", sdp, "packet", "realtime.input.packet -> realtime.sdp.writer.packet", edgePolicy, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
