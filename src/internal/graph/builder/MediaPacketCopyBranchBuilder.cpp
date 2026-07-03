#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {
namespace {

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

::media::Result<void> connectChecked(MediaGraph& graph,
                                     MediaNodeId fromNode,
                                     const std::string& fromPort,
                                     MediaNodeId toNode,
                                     const std::string& toPort,
                                     const std::string& label,
                                     std::size_t capacity)
{
    return MediaGraphBuildSupport::connectChecked(graph,
                                                  "MediaPacketCopyBranchBuilder",
                                                  fromNode,
                                                  fromPort,
                                                  toNode,
                                                  toPort,
                                                  label,
                                                  MediaGraphBuildSupport::blockingQueuePolicy(capacity));
}

} // namespace

::media::Result<void> MediaPacketCopyBranchBuilder::build(MediaGraph& graph,
                                                          const MediaPacketCopyBranchOptions& options)
{
    constexpr const char* owner = "MediaPacketCopyBranchBuilder";
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

    for (MediaNodeId nodeId : { sourceConfig, packetNormalize }) {
        if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph,
                                                                         owner,
                                                                         nodeId,
                                                                         options.streamKind,
                                                                         options.sourceStreamIndex); !status) {
            return status;
        }
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

    const MediaGraphQueueParameters& queues = options.queues;
    if (auto status = connectChecked(graph, options.formatSourceNode, options.formatSourcePort, sourceConfig, "format", prefix + ".format -> source_config.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, sourceConfig, "codec", options.muxNode, options.muxCodecPort, prefix + ".source_config.codec -> mux.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, options.formatSourceNode, options.formatSourcePort, packetNormalize, "format", prefix + ".format -> normalize.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, options.packetSourceNode, packetSourcePort, packetNormalize, "packet", prefix + ".packet -> normalize.packet", queues.packet); !status) return status;
    return connectChecked(graph, packetNormalize, "packet", options.muxNode, options.muxPacketPort, prefix + ".normalize.packet -> mux.packet", queues.mux);
}

} // namespace media::ffmpeg::graph
