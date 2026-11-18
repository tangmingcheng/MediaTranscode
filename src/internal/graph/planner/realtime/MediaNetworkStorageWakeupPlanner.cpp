#include "internal/graph/planner/realtime/MediaNetworkStorageWakeupPlanner.h"

#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"

#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {
namespace {
bool carries(const MediaGraph& graph, const MediaEdge& edge, MediaPayloadKind payload)
{
    const auto* from = graph.findPort(edge.from.portId);
    const auto* to = graph.findPort(edge.to.portId);
    return from && to && graph.findNode(edge.from.nodeId) && graph.findNode(edge.to.nodeId) &&
        from->nodeId == edge.from.nodeId && to->nodeId == edge.to.nodeId &&
        from->direction == MediaPortDirection::Output && to->direction == MediaPortDirection::Input &&
        from->payloadKind == payload && to->payloadKind == payload && edge.payloadKind == payload;
}
}

::media::Result<MediaNetworkStorageWakeupPlan> MediaNetworkStorageWakeupPlanner::plan(
    const MediaGraph& graph, MediaNodeId transportSource)
try {
    using Result = ::media::Result<MediaNetworkStorageWakeupPlan>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    const auto* source = graph.findNode(transportSource);
    if (!source || source->kind != MediaNodeKind::DatagramTransportPlanSource)
        return invalid("Storage notification requires its final transport source");
    auto transport = MediaDatagramTransportPlanSourceNodePlanCodec::decode(*source);
    if (!transport) return Result::failure(transport.error());
    const auto* sourcePlan = source->findOutputPort("plan");
    if (!sourcePlan || sourcePlan->payloadKind != MediaPayloadKind::DatagramTransportPlan)
        return invalid("Storage notification transport source lacks its typed plan port");
    MediaNetworkStorageWakeupPlan result{transportSource,
        MediaProtocolOutputSessionKey(transport.value().sessionKey()), {}};
    std::vector<MediaNodeId> direct;
    const auto append = [](std::vector<MediaNodeId>& values, MediaNodeId id) {
        if (std::find(values.begin(), values.end(), id) == values.end()) values.push_back(id);
    };
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId != transportSource || edge.from.portId != sourcePlan->id) continue;
        if (!carries(graph, edge, MediaPayloadKind::DatagramTransportPlan) ||
            MediaGraphShapeQuery::singleIncomingEdge(graph, edge.to.portId) != &edge)
            return invalid("Storage notification requires exact typed transport consumers");
        append(direct, edge.to.nodeId);
    }
    if (direct.empty()) return invalid("Storage notification has no transport consumers");
    result.producerTargets = direct;
    for (const auto consumer : direct) {
        const auto* forwarder = graph.findNode(consumer);
        if (forwarder->kind != MediaNodeKind::ProjectMpegTsPlanSource) continue;
        auto protocol = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(*forwarder);
        if (!protocol) return Result::failure(protocol.error());
        if (protocol.value().sessionKey != result.sessionKey)
            return invalid("Storage notification protocol plan belongs to another session");
        bool foundProducer = false;
        for (const auto& planEdge : graph.edges()) {
            if (planEdge.from.nodeId != consumer ||
                planEdge.payloadKind != MediaPayloadKind::ProjectMpegTsRuntimePlan) continue;
            if (!carries(graph, planEdge, MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
                MediaGraphShapeQuery::singleIncomingEdge(graph, planEdge.to.portId) != &planEdge)
                return invalid("Storage notification protocol resource forwarding is ambiguous");
            for (const auto& batchEdge : graph.edges()) {
                if (batchEdge.from.nodeId != planEdge.to.nodeId ||
                    batchEdge.payloadKind != MediaPayloadKind::MpegTsProtocolDatagramBatch) continue;
                if (!carries(graph, batchEdge, MediaPayloadKind::MpegTsProtocolDatagramBatch) ||
                    std::find(direct.begin(), direct.end(), batchEdge.to.nodeId) == direct.end())
                    return invalid("TS storage producer does not feed its transport consumer");
                append(result.producerTargets, planEdge.to.nodeId);
                foundProducer = true;
            }
        }
        if (!foundProducer) return invalid("Protocol resource forwarding has no typed TS storage producer");
    }
    std::sort(result.producerTargets.begin(), result.producerTargets.end(),
        [](MediaNodeId a, MediaNodeId b) { return a.value < b.value; });
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaNetworkStorageWakeupPlan>::failure(
        ::media::ErrorInfo::allocationFailed("Network storage notification planning"));
}

} // namespace media::ffmpeg::graph
