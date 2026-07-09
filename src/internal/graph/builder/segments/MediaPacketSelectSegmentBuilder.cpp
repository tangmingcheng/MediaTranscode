#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {

::media::Result<PacketSelectSegment> MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(
    MediaGraph& graph,
    const PacketSelectSegmentOptions& options)
{
    constexpr const char* owner = "MediaPacketSelectSegmentBuilder";
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<PacketSelectSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketSelectSegmentBuilder requires format source endpoint"));
    }
    if (options.queues.metadata == 0 || options.queues.packet == 0) {
        return ::media::Result<PacketSelectSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketSelectSegmentBuilder queue capacities must be greater than 0"));
    }

    PacketSelectSegment segment;
    segment.demux = graph.addNode(MediaNodeKind::Demux,
                                  options.prefix + ".demux",
                                  "Demux segment");
    segment.split = graph.addNode(MediaNodeKind::StreamSplit,
                                  options.prefix + ".stream.split",
                                  "Stream split segment");

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.demux,
                                                                  "format",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::FormatContext,
                                                                  true,
                                                                  false); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   segment.demux,
                                                                   "packet",
                                                                   MediaStreamKind::Any,
                                                                   MediaEdgeKind::InputPacket,
                                                                   MediaPayloadKind::Packet,
                                                                   true,
                                                                   true); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  segment.split,
                                                                  "packet",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::InputPacket,
                                                                  MediaPayloadKind::Packet,
                                                                  true,
                                                                  true); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }

    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            options.formatSourceNode,
                                                            options.formatSourcePort,
                                                             segment.demux,
                                                             "format",
                                                             options.prefix + ".format -> demux.format",
                                                             options.edgePolicies.metadata); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            segment.demux,
                                                            "packet",
                                                             segment.split,
                                                             "packet",
                                                             options.prefix + ".demux.packet -> stream.split.packet",
                                                             options.edgePolicies.packet); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }

    return ::media::Result<PacketSelectSegment>::success(segment);
}

} // namespace media::ffmpeg::graph
