#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaVideoPlanOptionApplier.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeOptionApplier.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoTranscodeBranchBuilder";

MediaVideoTranscodeBranchNodes addVideoTranscodeNodes(MediaGraph& graph,
                                                      const std::string& prefix,
                                                      bool inputStartRequiresKeyFrame)
{
    MediaVideoTranscodeBranchNodes nodes;
    nodes.codecResolver = graph.addNode(MediaNodeKind::CodecResolver, prefix + ".codec_resolver", "Video codec resolver");
    if (inputStartRequiresKeyFrame) {
        nodes.packetStartGate = graph.addNode(MediaNodeKind::PacketStartGate, prefix + ".packet_start_gate", "Video packet start gate");
    }
    nodes.videoDecode = graph.addNode(MediaNodeKind::VideoDecode, prefix + ".decode", "Video decode");
    nodes.hardwareTransfer = graph.addNode(MediaNodeKind::HardwareTransfer, prefix + ".hwtransfer", "Video hardware frame transfer");
    nodes.videoTimestamp = graph.addNode(MediaNodeKind::VideoTimestamp, prefix + ".timestamp", "Video timestamp normalize");
    nodes.videoFrameRate = graph.addNode(MediaNodeKind::VideoFrameRate, prefix + ".framerate", "Video frame rate control");
    nodes.videoFilter = graph.addNode(MediaNodeKind::VideoFilter, prefix + ".filter", "Video filter");
    nodes.videoEncode = graph.addNode(MediaNodeKind::VideoEncode, prefix + ".encode", "Video encode");
    return nodes;
}

::media::Result<void> addTranscodePorts(MediaGraph& graph,
                                        const MediaVideoTranscodeBranchOptions& options,
                                        const MediaVideoTranscodeBranchNodes& nodes)
{
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;

    const MediaPortId videoPort = graph.addOutputPort(options.packetSourceNode, options.packetSourcePort, MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(videoPort, owner, options.packetSourcePort); !status) return status;
    if (options.plan.sourceStreamIndex >= 0) {
        graph.setPortFormatDescriptor(videoPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Video, options.plan.sourceStreamIndex));
    }

    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "source_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
}

::media::Result<void> connectTranscodePorts(MediaGraph& graph,
                                            const MediaVideoTranscodeBranchOptions& options,
                                            const MediaVideoTranscodeBranchNodes& nodes)
{
    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.videoDecode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "timestamp_source", nodes.videoTimestamp, "source_codec", options.prefix + ".codec_resolver.timestamp_source -> timestamp.source_codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.videoTimestamp, "target_codec", options.prefix + ".codec_resolver.encoder -> timestamp.target_codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "target_codec", nodes.videoFilter, "codec", options.prefix + ".timestamp.target_codec -> filter.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", nodes.videoEncode, "codec", options.prefix + ".filter.codec -> encode.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", options.muxNode, options.muxCodecPort, options.prefix + ".filter.codec -> mux.codec", policies.metadata); !status) return status;
    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.packetStartGate, "packet", options.prefix + ".packet -> packet_start_gate.packet", policies.videoPacket); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.packetStartGate, "packet", nodes.videoDecode, "packet", options.prefix + ".packet_start_gate.packet -> decode.packet", policies.videoPacket); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.videoDecode, "packet", options.prefix + ".packet -> decode.packet", policies.videoPacket); !status) {
        return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoDecode, "frame", nodes.hardwareTransfer, "frame", options.prefix + ".decode.frame -> hwtransfer.frame", policies.frame); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.hardwareTransfer, "frame", nodes.videoTimestamp, "frame", options.prefix + ".hwtransfer.frame -> timestamp.frame", policies.frame); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "frame", nodes.videoFrameRate, "frame", options.prefix + ".timestamp.frame -> framerate.frame", policies.frame); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFrameRate, "frame", nodes.videoFilter, "frame", options.prefix + ".framerate.frame -> filter.frame", policies.frame); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "frame", nodes.videoEncode, "frame", options.prefix + ".filter.frame -> encode.frame", policies.frame); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoEncode, "packet", options.muxNode, options.muxPacketPort, options.prefix + ".encode.packet -> mux.packet", policies.videoMux);
}

} // namespace

::media::Result<void> MediaVideoTranscodeBranchBuilder::build(
    MediaGraph& graph,
    const MediaVideoTranscodeBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaVideoTranscodeBranchBuilder requires transcode_frame video branch"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeBranchBuilder requires planned video source stream index"));
    }

    MediaVideoTranscodeBranchNodes nodes = addVideoTranscodeNodes(graph,
                                                                  options.prefix,
                                                                  options.inputStartRequiresKeyFrame);
    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.packetStartGate, "packet_start_gate.require_key_frame", "1"); !status) return status;
    }
    if (auto status = MediaVideoTranscodeOptionApplier::applyUserOptions(graph, nodes, options.parameters); !status) return status;
    if (auto status = MediaVideoPlanOptionApplier::applySelectedPlan(graph, nodes, options.plan); !status) return status;
    if (auto status = addTranscodePorts(graph, options, nodes); !status) return status;
    return connectTranscodePorts(graph, options, nodes);
}

} // namespace media::ffmpeg::graph
