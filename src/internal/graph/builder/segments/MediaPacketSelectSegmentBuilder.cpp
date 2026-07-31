#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> addPlannedOutput(
    MediaGraph& graph,
    MediaNodeId split,
    const char* name,
    MediaStreamKind streamKind,
    const PacketSelectOutputPlan& plan)
{
    constexpr const char* owner = "MediaPacketSelectSegmentBuilder";
    if (plan.sourceStreamIndex < 0 ||
        (plan.edgeKind != MediaEdgeKind::InputPacket &&
         plan.edgeKind != MediaEdgeKind::EncodedPacket)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaPacketSelectSegmentBuilder requires complete output plans"));
    }
    const MediaPortId port = graph.addOutputPort(
        split, name, streamKind, plan.edgeKind, MediaPayloadKind::Packet,
        false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(port, owner, name); !status) {
        return status;
    }
    if (!graph.setPortFormatDescriptor(
            port, MediaGraphBuildSupport::streamIndexDescriptor(
                      streamKind, plan.sourceStreamIndex))) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(
                "MediaPacketSelectSegmentBuilder failed to set output descriptor"));
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<PacketSelectSegment> MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(
    MediaGraph& graph,
    const PacketSelectSegmentOptions& options)
{
    constexpr const char* owner = "MediaPacketSelectSegmentBuilder";
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<PacketSelectSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketSelectSegmentBuilder requires format source endpoint"));
    }
    if (options.metadataPolicy.queuePolicy.capacity == 0 ||
        options.packetPolicy.queuePolicy.capacity == 0) {
        return ::media::Result<PacketSelectSegment>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketSelectSegmentBuilder queue capacities must be greater than 0"));
    }
    if (!options.videoOutput && !options.audioOutput) {
        return ::media::Result<PacketSelectSegment>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaPacketSelectSegmentBuilder requires at least one explicit output plan"));
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
    if (options.videoOutput) {
        if (auto status = addPlannedOutput(
                graph, segment.split, "video", MediaStreamKind::Video,
                *options.videoOutput); !status) {
            return ::media::Result<PacketSelectSegment>::failure(status.error());
        }
        segment.videoPacket = MediaEndpoint{segment.split, "video"};
    }
    if (options.audioOutput) {
        if (auto status = addPlannedOutput(
                graph, segment.split, "audio", MediaStreamKind::Audio,
                *options.audioOutput); !status) {
            return ::media::Result<PacketSelectSegment>::failure(status.error());
        }
        segment.audioPacket = MediaEndpoint{segment.split, "audio"};
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
                                                             options.metadataPolicy); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            segment.demux,
                                                            "packet",
                                                             segment.split,
                                                             "packet",
                                                             options.prefix + ".demux.packet -> stream.split.packet",
                                                             options.packetPolicy); !status) {
        return ::media::Result<PacketSelectSegment>::failure(status.error());
    }

    return ::media::Result<PacketSelectSegment>::success(segment);
}

} // namespace media::ffmpeg::graph
