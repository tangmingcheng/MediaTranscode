#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph::MediaGraphBuildSupport {
namespace {

std::string ownedMessage(std::string_view owner, std::string_view action, std::string_view name)
{
    std::string message(owner);
    message += " failed to ";
    message += action;
    message += ": ";
    message += name;
    return message;
}

::media::Result<const char*> packetStreamKindOptionValue(MediaStreamKind streamKind)
{
    switch (streamKind) {
    case MediaStreamKind::Video:
        return ::media::Result<const char*>::success("video");
    case MediaStreamKind::Audio:
        return ::media::Result<const char*>::success("audio");
    default:
        return ::media::Result<const char*>::failure(
            ::media::ErrorInfo::invalidArgument("packet stream kind must be video or audio"));
    }
}

} // namespace

MediaEdgePolicy blockingQueuePolicy(std::size_t capacity) noexcept
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

MediaFormatDescriptor streamIndexDescriptor(MediaStreamKind streamKind, int streamIndex) noexcept
{
    MediaFormatDescriptor descriptor;
    descriptor.streamKind = streamKind;
    descriptor.streamIndex = streamIndex;
    return descriptor;
}

::media::Result<void> setNodeOptionChecked(MediaGraph& graph,
                                           std::string_view owner,
                                           MediaNodeId nodeId,
                                           std::string_view key,
                                           std::string_view value)
{
    const std::string optionKey(key);
    const std::string optionValue(value);
    if (!graph.setNodeOption(nodeId, optionKey, optionValue)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(ownedMessage(owner, "set option", key)));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> setPacketStreamOptions(MediaGraph& graph,
                                             std::string_view owner,
                                             MediaNodeId nodeId,
                                             MediaStreamKind streamKind,
                                             int sourceStreamIndex)
{
    if (sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(ownedMessage(owner, "set packet stream index", "negative source stream index")));
    }

    auto streamKindValue = packetStreamKindOptionValue(streamKind);
    if (!streamKindValue) {
        return ::media::Result<void>::failure(streamKindValue.error());
    }

    if (auto status = setNodeOptionChecked(graph,
                                           owner,
                                           nodeId,
                                           MediaTranscodeOptionKey::PacketSourceStreamIndex,
                                           std::to_string(sourceStreamIndex)); !status) {
        return status;
    }
    return setNodeOptionChecked(graph,
                                owner,
                                nodeId,
                                MediaTranscodeOptionKey::PacketStreamKind,
                                streamKindValue.value());
}

::media::Result<void> requirePort(MediaPortId portId,
                                  std::string_view owner,
                                  std::string_view name)
{
    if (!portId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(ownedMessage(owner, "add port", name)));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> requireEdge(MediaEdgeId edgeId,
                                  std::string_view owner,
                                  std::string_view name)
{
    if (!edgeId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(ownedMessage(owner, "connect edge", name)));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> addInputPortChecked(MediaGraph& graph,
                                          std::string_view owner,
                                          MediaNodeId nodeId,
                                          std::string name,
                                          MediaStreamKind streamKind,
                                          MediaEdgeKind edgeKind,
                                          MediaPayloadKind payloadKind,
                                          bool required,
                                          bool multiple)
{
    const std::string portName = name;
    return requirePort(graph.addInputPort(nodeId,
                                          std::move(name),
                                          streamKind,
                                          edgeKind,
                                          payloadKind,
                                          required,
                                          multiple),
                       owner,
                       portName);
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph,
                                           std::string_view owner,
                                           MediaNodeId nodeId,
                                           std::string name,
                                           MediaStreamKind streamKind,
                                           MediaEdgeKind edgeKind,
                                           MediaPayloadKind payloadKind,
                                           bool required,
                                           bool multiple)
{
    const std::string portName = name;
    return requirePort(graph.addOutputPort(nodeId,
                                           std::move(name),
                                           streamKind,
                                           edgeKind,
                                           payloadKind,
                                           required,
                                           multiple),
                       owner,
                       portName);
}

::media::Result<void> connectChecked(MediaGraph& graph,
                                     std::string_view owner,
                                     MediaNodeId fromNode,
                                     const std::string& fromPort,
                                     MediaNodeId toNode,
                                     const std::string& toPort,
                                     std::string label,
                                     const MediaEdgePolicy& policy,
                                     bool required)
{
    const std::string edgeName = label;
    return requireEdge(graph.connect(fromNode,
                                     fromPort,
                                     toNode,
                                     toPort,
                                     std::move(label),
                                     policy,
                                     required),
                       owner,
                       edgeName);
}

} // namespace media::ffmpeg::graph::MediaGraphBuildSupport
