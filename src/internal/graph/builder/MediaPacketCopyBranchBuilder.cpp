#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

#include <array>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaEdgePolicy blockingQueuePolicy(std::size_t capacity)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    policy.queuePolicy.preserveOrdering = true;
    policy.queuePolicy.allowFlushControlBypass = true;
    policy.queuePolicy.collectMetrics = true;
    return policy;
}

const char* streamKindOptionName(MediaStreamKind streamKind) noexcept
{
    switch (streamKind) {
    case MediaStreamKind::Video: return "video";
    case MediaStreamKind::Audio: return "audio";
    default: return "unknown";
    }
}

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

MediaFormatDescriptor streamIndexDescriptor(MediaStreamKind streamKind, int streamIndex)
{
    MediaFormatDescriptor descriptor;
    descriptor.streamKind = streamKind;
    descriptor.streamIndex = streamIndex;
    return descriptor;
}

::media::Result<void> requirePort(MediaPortId portId, const char* name)
{
    if (!portId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(std::string("MediaPacketCopyBranchBuilder failed to add port: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> requireEdge(MediaEdgeId edgeId, const char* name)
{
    if (!edgeId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(std::string("MediaPacketCopyBranchBuilder failed to connect edge: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> setNodeOptionChecked(MediaGraph& graph,
                                           MediaNodeId nodeId,
                                           const std::string& key,
                                           const std::string& value)
{
    if (!graph.setNodeOption(nodeId, key, value)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError("MediaPacketCopyBranchBuilder failed to set option: " + key));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> addInputPortChecked(MediaGraph& graph,
                                          MediaNodeId nodeId,
                                          const std::string& name,
                                          MediaStreamKind streamKind,
                                          MediaEdgeKind edgeKind,
                                          MediaPayloadKind payloadKind,
                                          bool required,
                                          bool multiple)
{
    return requirePort(graph.addInputPort(nodeId,
                                          name,
                                          streamKind,
                                          edgeKind,
                                          payloadKind,
                                          required,
                                          multiple),
                       name.c_str());
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph,
                                           MediaNodeId nodeId,
                                           const std::string& name,
                                           MediaStreamKind streamKind,
                                           MediaEdgeKind edgeKind,
                                           MediaPayloadKind payloadKind,
                                           bool required,
                                           bool multiple)
{
    return requirePort(graph.addOutputPort(nodeId,
                                           name,
                                           streamKind,
                                           edgeKind,
                                           payloadKind,
                                           required,
                                           multiple),
                       name.c_str());
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

    for (MediaNodeId nodeId : { sourceConfig, packetNormalize }) {
        if (auto status = setNodeOptionChecked(graph,
                                               nodeId,
                                               MediaTranscodeOptionKey::PacketSourceStreamIndex,
                                               std::to_string(options.sourceStreamIndex)); !status) {
            return status;
        }
        if (auto status = setNodeOptionChecked(graph,
                                               nodeId,
                                               MediaTranscodeOptionKey::PacketStreamKind,
                                               streamKindOptionName(options.streamKind)); !status) {
            return status;
        }
    }

    if (auto status = addInputPortChecked(graph,
                                          sourceConfig,
                                          "format",
                                          MediaStreamKind::Metadata,
                                          MediaEdgeKind::Metadata,
                                          MediaPayloadKind::FormatContext,
                                          true,
                                          false); !status) return status;
    if (auto status = addOutputPortChecked(graph,
                                           sourceConfig,
                                           "codec",
                                           options.streamKind,
                                           MediaEdgeKind::Metadata,
                                           MediaPayloadKind::CodecParameters,
                                           true,
                                           false); !status) return status;
    if (auto status = addInputPortChecked(graph,
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
    if (auto status = requirePort(packetSource, packetSourcePort.c_str()); !status) return status;
    graph.setPortFormatDescriptor(packetSource, streamIndexDescriptor(options.streamKind, options.sourceStreamIndex));

    if (auto status = addInputPortChecked(graph,
                                          packetNormalize,
                                          "packet",
                                          options.streamKind,
                                          MediaEdgeKind::InputPacket,
                                          MediaPayloadKind::Packet,
                                          true,
                                          true); !status) return status;
    if (auto status = addOutputPortChecked(graph,
                                           packetNormalize,
                                           "packet",
                                           options.streamKind,
                                           MediaEdgeKind::EncodedPacket,
                                           MediaPayloadKind::Packet,
                                           true,
                                           true); !status) return status;

    const MediaGraphQueueParameters& queues = options.queues;
    const std::array<std::pair<MediaEdgeId, const char*>, 5> edges {{
        { graph.connect(options.formatSourceNode, options.formatSourcePort, sourceConfig, "format", prefix + ".format -> source_config.format", blockingQueuePolicy(queues.metadata)), "source_config.format" },
        { graph.connect(sourceConfig, "codec", options.muxNode, options.muxCodecPort, prefix + ".source_config.codec -> mux.codec", blockingQueuePolicy(queues.metadata)), "source_config.codec" },
        { graph.connect(options.formatSourceNode, options.formatSourcePort, packetNormalize, "format", prefix + ".format -> normalize.format", blockingQueuePolicy(queues.metadata)), "normalize.format" },
        { graph.connect(options.packetSourceNode, packetSourcePort, packetNormalize, "packet", prefix + ".packet -> normalize.packet", blockingQueuePolicy(queues.packet)), "normalize.packet" },
        { graph.connect(packetNormalize, "packet", options.muxNode, options.muxPacketPort, prefix + ".normalize.packet -> mux.packet", blockingQueuePolicy(queues.mux)), "normalize.mux" },
    }};
    for (const auto& edge : edges) {
        if (auto status = requireEdge(edge.first, edge.second); !status) return status;
    }

    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
