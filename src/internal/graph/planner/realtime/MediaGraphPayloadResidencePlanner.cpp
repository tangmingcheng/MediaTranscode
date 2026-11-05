#include "internal/graph/planner/realtime/MediaGraphPayloadResidencePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {
namespace {
::media::Status validateResidenceTopology(const MediaPayloadResidenceTopology& topology,
    std::size_t producerPosition)
{
    if (producerPosition >= topology.nodeCount)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Residence producer position is absent"));
    for (const auto& edge : topology.connections)
        if (edge.from >= topology.nodeCount || edge.to >= topology.nodeCount)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Residence connection position is absent"));
    for (const auto& owner : topology.owners)
        for (const auto position : owner.retainingPositions)
            if (position >= topology.nodeCount)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Residence owner position is absent"));
    return ::media::Status::success();
}
}

bool MediaPayloadResidenceEvaluation::retains(const MediaPayloadResidenceOwner& owner) const noexcept
{
    return owner.maximumReferences && std::any_of(owner.retainingPositions.begin(),
        owner.retainingPositions.end(), [&](std::size_t position) {
            return std::find(reachablePositions.begin(), reachablePositions.end(), position) != reachablePositions.end();
        });
}

::media::Result<MediaPayloadResidenceEvaluation> MediaGraphPayloadResidencePlanner::evaluate(
    const MediaPayloadResidenceTopology& topology, std::size_t producerPosition,
    std::span<std::size_t> workspace)
{
    using Result = ::media::Result<MediaPayloadResidenceEvaluation>;
    if (auto status = validateResidenceTopology(topology, producerPosition); !status)
        return Result::failure(status.error());
    if (workspace.size() < topology.nodeCount)
        return Result::failure(::media::ErrorInfo::invalidArgument("Residence traversal workspace is too small"));
    std::size_t count = 1;
    workspace[0] = producerPosition;
    const auto contains = [&](std::size_t position) {
        const auto reachable = workspace.first(count);
        return std::find(reachable.begin(), reachable.end(), position) != reachable.end();
    };
    // A downstream producer may alias input; it is not an ownership cut.
    for (std::size_t i = 0; i < count; ++i)
        for (const auto& edge : topology.connections)
            if (edge.from == workspace[i] && !contains(edge.to))
                workspace[count++] = edge.to;
    MediaPayloadResidenceEvaluation result{workspace.first(count), 0};
    for (const auto& owner : topology.owners) {
        if (!result.retains(owner)) continue;
        auto sum = MediaCheckedArithmetic::add(result.maximumRetainedObjects, owner.maximumReferences,
            "producer reachable retention credits");
        if (!sum) return Result::failure(sum.error());
        result.maximumRetainedObjects = sum.value();
    }
    return Result::success(result);
}

::media::Result<MediaPayloadResidenceProjection> MediaGraphPayloadResidencePlanner::plan(
    const MediaPayloadResidenceTopology& topology, std::size_t producerPosition)
try {
    using Result = ::media::Result<MediaPayloadResidenceProjection>;
    if (auto status = validateResidenceTopology(topology, producerPosition); !status)
        return Result::failure(status.error());
    MediaPayloadResidenceProjection result{{}, 0, {}};
    if (topology.nodeCount > result.reachablePositions.max_size())
        return Result::failure(::media::ErrorInfo::allocationFailed("Residence traversal exceeds addressable workspace"));
    result.reachablePositions.resize(topology.nodeCount);
    auto evaluated = evaluate(topology, producerPosition, result.reachablePositions);
    if (!evaluated) return Result::failure(evaluated.error());
    result.maximumRetainedObjects = evaluated.value().maximumRetainedObjects;
    result.reachablePositions.resize(evaluated.value().reachablePositions.size());
    for (const auto& owner : topology.owners)
        if (evaluated.value().retains(owner)) result.contributions.push_back(owner);
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPayloadResidenceProjection>::failure(
        ::media::ErrorInfo::allocationFailed("payload residence topology traversal"));
}

::media::Result<MediaGraphPayloadResidence> MediaGraphPayloadResidencePlanner::plan(
    const MediaGraph& graph, const MediaGraphPayloadProducerKey& producer,
    const MediaGraphPayloadResidenceFacts& facts)
try {
    using Result = ::media::Result<MediaGraphPayloadResidence>;
    if (!graph.findNode(producer.nodeId) ||
        (producer.payloadKind != MediaPayloadKind::Packet && producer.payloadKind != MediaPayloadKind::Frame) ||
        (producer.streamKind != MediaStreamKind::Video && producer.streamKind != MediaStreamKind::Audio))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Payload residence requires an existing audio/video packet/frame producer"));

    std::vector<MediaNodeId> positions;
    for (const auto& node : graph.nodes()) positions.push_back(node.id);
    const auto position = [&](MediaNodeId id) {
        return static_cast<std::size_t>(std::find(positions.begin(), positions.end(), id) - positions.begin());
    };
    std::vector<MediaEdgeId> edges;
    for (const auto& edge : graph.edges()) edges.push_back(edge.id);
    const auto connection = [&](MediaEdgeId id) {
        return static_cast<std::size_t>(std::find(edges.begin(), edges.end(), id) - edges.begin());
    };
    MediaPayloadResidenceTopology topology{positions.size(), {}, {}};
    const auto mediaUnits = [&](const auto& bound) -> std::optional<std::uint64_t> {
        if (producer.streamKind == MediaStreamKind::Video && bound.videoPayload == producer.payloadKind)
            return bound.videoUnits;
        if (producer.streamKind == MediaStreamKind::Audio && producer.payloadKind == MediaPayloadKind::Packet)
            return bound.audioPacketUnits;
        return std::nullopt;
    };
    const auto eventFor = [&](MediaEdgeId id) {
        return std::find_if(facts.startupEvents.begin(), facts.startupEvents.end(),
            [&](const auto& value) { return value.edge == id; });
    };
    const auto carriesPayload = [&](const MediaEdge& edge) {
        const auto event = eventFor(edge.id);
        if (event != facts.startupEvents.end()) return mediaUnits(*event).has_value();
        return edge.payloadKind == producer.payloadKind && edge.streamKind == producer.streamKind;
    };
    for (const auto& edge : graph.edges()) {
        if (!carriesPayload(edge)) continue;
        if (!graph.findNode(edge.from.nodeId) || !graph.findNode(edge.to.nodeId))
            return Result::failure(::media::ErrorInfo::invalidArgument("Payload residence edge endpoint is absent"));
        topology.connections.push_back({position(edge.from.nodeId), position(edge.to.nodeId)});
    }
    for (const auto& bound : facts.startupEvents) {
        const auto* edge = graph.findEdge(bound.edge);
        if (!edge) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Payload residence startup event edge is missing"));
        const auto units = mediaUnits(bound);
        if (!units) continue;
        auto objects = MediaCheckedArithmetic::multiply(bound.wrapperSlots, *units,
            "startup event queue media retention");
        if (!objects) return Result::failure(objects.error());
        topology.owners.push_back({{position(edge->from.nodeId)}, objects.value(),
            MediaPayloadResidenceOwnerKind::StartupEdge, connection(bound.edge)});
    }
    for (const auto& bound : facts.startupNodes) {
        if (!graph.findNode(bound.node)) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Payload residence startup node is missing"));
        const auto units = mediaUnits(bound);
        if (units) topology.owners.push_back({{position(bound.node)}, *units,
            MediaPayloadResidenceOwnerKind::StartupNode, std::nullopt});
    }
    for (const auto& entry : facts.storage) {
        if (!entry.validReferenceIdentity())
            return Result::failure(::media::ErrorInfo::invalidArgument("Payload residence storage has conflicting reference identities"));
        // Typed startup products replace the corresponding mixed entries.
        // Do not multiply node port slots and already-counted media by a batch.
        const bool typedNode = std::any_of(facts.startupNodes.begin(), facts.startupNodes.end(),
            [&](const auto& bound) { return entry.referenceNode == bound.node; });
        const bool typedEdge = std::any_of(facts.startupEvents.begin(), facts.startupEvents.end(),
            [&](const auto& bound) { return entry.referenceEdge == bound.edge; });
        if (typedNode || typedEdge) continue;
        MediaPayloadResidenceOwner owner{{}, entry.maximumBufferObjects,
            MediaPayloadResidenceOwnerKind::Shared, std::nullopt};
        if (!entry.retentionNodes.empty()) {
            // One storage entry can serve several retaining nodes. Charge it
            // once if any reachable owner can retain this producer's payload.
            for (const auto id : entry.retentionNodes) {
                if (!graph.findNode(id)) return Result::failure(
                    ::media::ErrorInfo::invalidArgument("Payload residence storage has no retaining node"));
                owner.retainingPositions.push_back(position(id));
            }
        } else if (entry.referenceNode) {
            owner.kind = MediaPayloadResidenceOwnerKind::Node;
            if (!graph.findNode(*entry.referenceNode)) return Result::failure(
                ::media::ErrorInfo::invalidArgument("Payload residence ordinary node is absent"));
            owner.retainingPositions.push_back(position(*entry.referenceNode));
        } else if (entry.referenceEdge) {
            owner.kind = MediaPayloadResidenceOwnerKind::Edge;
            owner.connection = connection(*entry.referenceEdge);
            const auto* edge=graph.findEdge(*entry.referenceEdge);
            if (!edge) return Result::failure(::media::ErrorInfo::invalidArgument("Payload residence ordinary edge is absent"));
            if (carriesPayload(*edge)) owner.retainingPositions.push_back(position(edge->from.nodeId));
        }
        topology.owners.push_back(std::move(owner));
    }
    auto projected = plan(topology, position(producer.nodeId));
    if (!projected) return Result::failure(projected.error());
    MediaGraphPayloadResidence result{{}, projected.value().maximumRetainedObjects, {}};
    for (const auto index : projected.value().reachablePositions)
        result.reachableNodes.push_back(positions[index]);
    for (const auto& owner : projected.value().contributions) {
        MediaGraphPayloadResidenceContribution contribution{{}, owner.maximumReferences, owner.kind, std::nullopt};
        for (const auto index : owner.retainingPositions)
            contribution.retainingNodes.push_back(positions[index]);
        if (owner.connection) contribution.edge = edges[*owner.connection];
        result.contributions.push_back(std::move(contribution));
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaGraphPayloadResidence>::failure(
        ::media::ErrorInfo::allocationFailed("payload residence graph traversal"));
}

} // namespace media::ffmpeg::graph
