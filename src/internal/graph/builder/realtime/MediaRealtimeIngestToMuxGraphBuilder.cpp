#include "internal/graph/builder/realtime/MediaRealtimeIngestToMuxGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeEdgePolicy.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeIngestToMuxGraphBuilder";

} // namespace

::media::Result<MediaGraph> MediaRealtimeIngestToMuxGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = MediaRealtimeEdgePolicy::make(options);

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput, "realtime.input", "Realtime packet input");
    const MediaNodeId fanout = graph.addNode(MediaNodeKind::PacketFanout, "realtime.packet.fanout", "Realtime packet fanout");
    const MediaNodeId mux = graph.addNode(options.enableRtpMux ? MediaNodeKind::RtpMux : MediaNodeKind::PacketMerge,
                                          options.enableRtpMux ? "realtime.rtp.mux" : "realtime.packet.merge",
                                          options.enableRtpMux ? "Realtime RTP mux" : "Realtime packet merge");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput, "realtime.rtp.output", "Realtime RTP output");

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, output, options); !status) return ::media::Result<MediaGraph>::failure(status.error());

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

} // namespace media::ffmpeg::graph
