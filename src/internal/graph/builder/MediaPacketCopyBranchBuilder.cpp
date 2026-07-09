#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaPacketCopyBranchBuilder";

std::string defaultPrefix(MediaStreamKind streamKind)
{
    switch (streamKind) {
    case MediaStreamKind::Video: return "packet.video";
    case MediaStreamKind::Audio: return "packet.audio";
    default: return "packet.unknown";
    }
}

std::string defaultPacketSourcePort(MediaStreamKind streamKind)
{
    switch (streamKind) {
    case MediaStreamKind::Video: return "video";
    case MediaStreamKind::Audio: return "audio";
    default: return {};
    }
}

::media::Result<void> validateOptions(const MediaPacketCopyBranchOptions& options)
{
    if (options.streamKind != MediaStreamKind::Video && options.streamKind != MediaStreamKind::Audio) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires audio or video stream kind"));
    }
    if (options.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires non-negative source stream index"));
    }
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires format source endpoint"));
    }
    if (!options.packetSourceNode.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires packet source node"));
    }
    if (!options.muxNode.isValid() || options.muxCodecPort.empty() || options.muxPacketPort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires mux endpoints"));
    }
    if (options.queues.metadata == 0 || options.queues.packet == 0 || options.queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder queue capacities must be greater than 0"));
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<void> MediaPacketCopyBranchBuilder::build(MediaGraph& graph,
                                                          const MediaPacketCopyBranchOptions& options)
{
    if (auto status = validateOptions(options); !status) {
        return status;
    }

    const std::string prefix = options.prefix.empty() ? defaultPrefix(options.streamKind) : options.prefix;
    const std::string packetSourcePort = options.packetSourcePort.empty()
        ? defaultPacketSourcePort(options.streamKind)
        : options.packetSourcePort;
    if (packetSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPacketCopyBranchBuilder requires packet source port"));
    }

    const MediaNodeId sourceConfig = graph.addNode(MediaNodeKind::PacketSourceConfig,
                                                  prefix + ".source_config",
                                                  "Packet source config");
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize,
                                                     prefix + ".normalize",
                                                     "Packet normalize");

    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph,
                                                                     owner,
                                                                     sourceConfig,
                                                                     options.streamKind,
                                                                     options.sourceStreamIndex); !status) {
        return status;
    }
    if (auto status = MediaGraphBuildSupport::setPacketNormalizeOptions(graph,
                                                                        owner,
                                                                        packetNormalize,
                                                                        options.streamKind,
                                                                        options.sourceStreamIndex,
                                                                        options.monotonicPacketTimestamps); !status) {
        return status;
    }

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  sourceConfig,
                                                                  "format",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::FormatContext,
                                                                  true,
                                                                  false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   sourceConfig,
                                                                   "codec",
                                                                   options.streamKind,
                                                                   MediaEdgeKind::Metadata,
                                                                   MediaPayloadKind::CodecParameters,
                                                                   true,
                                                                   false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  packetNormalize,
                                                                  "format",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::FormatContext,
                                                                  true,
                                                                  false); !status) return status;

    const MediaPortId packetSource = graph.addOutputPort(options.packetSourceNode,
                                                         packetSourcePort,
                                                         options.streamKind,
                                                         MediaEdgeKind::InputPacket,
                                                         MediaPayloadKind::Packet,
                                                         false,
                                                         true);
    if (auto status = MediaGraphBuildSupport::requirePort(packetSource, owner, packetSourcePort); !status) return status;
    graph.setPortFormatDescriptor(packetSource,
                                  MediaGraphBuildSupport::streamIndexDescriptor(options.streamKind,
                                                                                options.sourceStreamIndex));

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  packetNormalize,
                                                                  "packet",
                                                                  options.streamKind,
                                                                  MediaEdgeKind::InputPacket,
                                                                  MediaPayloadKind::Packet,
                                                                  true,
                                                                  true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   packetNormalize,
                                                                   "packet",
                                                                   options.streamKind,
                                                                   MediaEdgeKind::EncodedPacket,
                                                                   MediaPayloadKind::Packet,
                                                                   true,
                                                                   true); !status) return status;

    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    const MediaEdgePolicy& packetPolicy = policies.packetPolicy(options.streamKind);
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, sourceConfig, "format", prefix + ".format -> source_config.format", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, sourceConfig, "codec", options.muxNode, options.muxCodecPort, prefix + ".source_config.codec -> mux.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, packetNormalize, "format", prefix + ".format -> normalize.format", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, packetSourcePort, packetNormalize, "packet", prefix + ".packet -> normalize.packet", packetPolicy); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, packetNormalize, "packet", options.muxNode, options.muxPacketPort, prefix + ".normalize.packet -> mux.packet", policies.mux);
}

} // namespace media::ffmpeg::graph
