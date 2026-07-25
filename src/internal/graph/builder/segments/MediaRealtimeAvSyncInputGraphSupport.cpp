#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaRealtimeAvSyncInputSegmentBuilder";

} // namespace

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::setOption(
    MediaGraph& graph,
    MediaNodeId node,
    std::string_view key,
    std::string value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, node, key, std::move(value));
}

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::addInput(
    MediaGraph& graph,
    MediaNodeId node,
    std::string name,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload)
{
    return MediaGraphBuildSupport::addInputPortChecked(
        graph, Owner, node, std::move(name), stream, edge, payload, true, false);
}

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::addOutput(
    MediaGraph& graph,
    MediaNodeId node,
    std::string name,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload,
    bool required,
    bool multiple)
{
    return MediaGraphBuildSupport::addOutputPortChecked(
        graph, Owner, node, std::move(name), stream, edge, payload,
        required, multiple);
}

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::validateOutput(
    const MediaGraph& graph,
    const MediaEndpoint& endpoint,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload)
{
    if (!graph.findNode(endpoint.node) || endpoint.port.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input segment requires valid source endpoints"));
    }
    const MediaPort* existing = graph.findOutputPort(
        endpoint.node, endpoint.port);
    if (!existing) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input source endpoint does not exist"));
    }
    if (existing->streamKind != stream || existing->edgeKind != edge ||
        existing->payloadKind != payload || !existing->multiple) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input source endpoint does not match its planned type"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::connect(
    MediaGraph& graph,
    const MediaEndpoint& from,
    MediaNodeId to,
    std::string toPort,
    std::string label,
    const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, from.node, from.port, to, toPort, std::move(label),
        policy);
}

::media::Result<void> MediaRealtimeAvSyncInputGraphSupport::connect(
    MediaGraph& graph,
    MediaNodeId from,
    std::string fromPort,
    MediaNodeId to,
    std::string toPort,
    std::string label,
    const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, from, fromPort, to, toPort, std::move(label), policy);
}

::media::Result<MediaNodeId> MediaRealtimeAvSyncInputGraphSupport::addNode(
    MediaGraph& graph,
    MediaNodeKind kind,
    std::string name,
    std::string diagnostic)
{
    MediaNodeId node = graph.addNode(
        kind, std::move(name), std::move(diagnostic));
    if (!node.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError(
                "Synchronized input segment failed to add a node"));
    }
    return ::media::Result<MediaNodeId>::success(node);
}

} // namespace media::ffmpeg::graph
