#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaAudioEncodeOptionApplier.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioEncodeBranchBuilder";

struct MediaAudioEncodeBranchNodes {
    MediaNodeId packetNormalize = MediaNodeId::invalid();
    MediaNodeId codecResolver = MediaNodeId::invalid();
    MediaNodeId decode = MediaNodeId::invalid();
    MediaNodeId resample = MediaNodeId::invalid();
    MediaNodeId encode = MediaNodeId::invalid();
};

MediaAudioEncodeBranchNodes addAudioEncodeNodes(MediaGraph& graph,
                                                const std::string& prefix)
{
    MediaAudioEncodeBranchNodes nodes;
    nodes.packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize, prefix + ".packet_normalize", "Audio packet normalize");
    nodes.codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, prefix + ".codec_resolver", "Audio codec resolver");
    nodes.decode = graph.addNode(MediaNodeKind::AudioDecode, prefix + ".decode", "Audio decode");
    nodes.resample = graph.addNode(MediaNodeKind::AudioResample, prefix + ".resample", "Audio resample");
    nodes.encode = graph.addNode(MediaNodeKind::AudioEncode, prefix + ".encode", "Audio encode");
    return nodes;
}

::media::Result<void> addEncodePorts(MediaGraph& graph,
                                     const MediaAudioEncodeBranchOptions& options,
                                     const MediaAudioEncodeBranchNodes& nodes)
{
    const MediaPortId audioPort = graph.addOutputPort(options.packetSourceNode, options.packetSourcePort, MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(audioPort, owner, options.packetSourcePort); !status) return status;
    graph.setPortFormatDescriptor(audioPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, options.plan.sourceStreamIndex));

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.decode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.decode, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.decode, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.encode, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.encode, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
}

::media::Result<void> connectEncodePorts(MediaGraph& graph,
                                         const MediaAudioEncodeBranchOptions& options,
                                         const MediaAudioEncodeBranchNodes& nodes)
{
    const MediaGraphQueueParameters& queues = options.queues;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.packetNormalize, "format", options.prefix + ".format -> packet_normalize.format", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.packetNormalize, "packet", options.prefix + ".packet -> packet_normalize.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.packet)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.decode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.packetNormalize, "packet", nodes.decode, "packet", options.prefix + ".packet_normalize.packet -> decode.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.packet)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.resample, "codec", options.prefix + ".codec_resolver.encoder -> resample.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.decode, "frame", nodes.resample, "frame", options.prefix + ".decode.frame -> resample.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.encode, "codec", options.prefix + ".codec_resolver.encoder -> encode.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.resample, "frame", nodes.encode, "frame", options.prefix + ".resample.frame -> encode.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.encode, "codec", options.muxNode, options.muxCodecPort, options.prefix + ".encode.codec -> mux.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, nodes.encode, "packet", options.muxNode, options.muxPacketPort, options.prefix + ".encode.packet -> mux.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.mux));
}

} // namespace

::media::Result<void> MediaAudioEncodeBranchBuilder::build(
    MediaGraph& graph,
    const MediaAudioEncodeBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioEncodeBranchBuilder requires transcode_frame audio branch"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires planned audio source stream index"));
    }

    const MediaAudioEncodeBranchNodes nodes = addAudioEncodeNodes(graph, options.prefix);
    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph, owner, nodes.packetNormalize, MediaStreamKind::Audio, options.plan.sourceStreamIndex); !status) return status;
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver, MediaTranscodeOptionKey::AudioSourceStreamIndex, std::to_string(options.plan.sourceStreamIndex)); !status) return status;
    if (auto status = MediaAudioEncodeOptionApplier::applyCodecResolverOptions(graph, nodes.codecResolver, options.parameters, options.plan); !status) return status;
    if (auto status = addEncodePorts(graph, options, nodes); !status) return status;
    return connectEncodePorts(graph, options, nodes);
}

} // namespace media::ffmpeg::graph
