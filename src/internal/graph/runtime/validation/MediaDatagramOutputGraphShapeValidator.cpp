#include "internal/graph/runtime/validation/MediaDatagramOutputGraphShapeValidator.h"

#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"
#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

bool sameEndpoint(const MediaDatagramRemoteEndpointFact& left,
                  const MediaDatagramRemoteEndpointFact& right) noexcept
{
    return left.endpointId == right.endpointId && left.role == right.role &&
        left.addressFamily == right.addressFamily &&
        left.numericAddress == right.numericAddress &&
        left.port == right.port;
}

} // namespace

::media::Status MediaDatagramOutputGraphShapeValidator::validate(
    const MediaGraph& graph,
    const MediaDatagramTransportPlanTemplate& planTemplate,
    MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaNodeKind materializerKind,
    std::size_t materializerCount,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    const MediaAvSyncGraphShape shape(graph);
    auto exact = shape.requireExact({
        {MediaNodeKind::DatagramTransportPlanSource, 1,
         "datagram transport plan source"},
        {MediaNodeKind::ScheduledDatagramSender, 1,
         "common GCRA-paced datagram sender"},
        {materializerKind, materializerCount,
         "protocol datagram materializer"}} ,
        "common datagram output execution shape");
    if (!exact) return exact;
    if (!sessionKey.valid() || planTemplate.sessionKey() != sessionKey.value()) {
        return invalid("Datagram execution session differs from its planner product");
    }
    const MediaNode& source =
        *shape.nodes(MediaNodeKind::DatagramTransportPlanSource).front();
    const MediaNode& sender =
        *shape.nodes(MediaNodeKind::ScheduledDatagramSender).front();
    auto decoded = MediaDatagramTransportPlanSourceNodePlanCodec::decode(source);
    if (!decoded) return ::media::Status::failure(decoded.error());
    if (decoded.value().sessionKey() != planTemplate.sessionKey() ||
        decoded.value().serviceScopeId() != planTemplate.serviceScopeId() ||
        decoded.value().encode().deployment !=
            planTemplate.encode().deployment ||
        decoded.value().remoteEndpoints().size() !=
            planTemplate.remoteEndpoints().size()) {
        return invalid("Datagram transport source differs from its planner product");
    }
    for (std::size_t index = 0;
         index < planTemplate.remoteEndpoints().size(); ++index) {
        if (!sameEndpoint(decoded.value().remoteEndpoints()[index],
                          planTemplate.remoteEndpoints()[index])) {
            return invalid("Datagram endpoint facts differ from their planner product");
        }
    }
    auto encodedSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    if (!encodedSet || sender.options.values().size() != 2 ||
        sender.options.value("scheduled_datagram_sender.session") !=
            sessionKey.value() ||
        sender.options.value("scheduled_datagram_sender.stream_set") !=
            encodedSet.value()) {
        return invalid("Common datagram sender identity differs from its planner product");
    }
    const MediaPort* sourceActivation = source.findInputPort("activation");
    const MediaPort* sourcePlan = source.findOutputPort("plan");
    const MediaPort* senderPlan = sender.findInputPort("plan");
    const MediaPort* senderBatch = sender.findInputPort("batch");
    if (source.inputPorts.size() != 1 || source.outputPorts.size() != 1 ||
        sender.inputPorts.size() != 2 || !sender.outputPorts.empty() ||
        !MediaGraphShapeQuery::validPort(sourceActivation,
            MediaPortDirection::Input, MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent) ||
        !MediaGraphShapeQuery::validPort(sourcePlan,
            MediaPortDirection::Output, MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::DatagramTransportPlan) ||
        !MediaGraphShapeQuery::validPort(senderPlan,
            MediaPortDirection::Input, MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::DatagramTransportPlan) ||
        !MediaGraphShapeQuery::validPort(senderBatch,
            MediaPortDirection::Input, MediaStreamKind::Metadata,
            MediaEdgeKind::ScheduledDatagramBatch,
            MediaPayloadKind::WireDatagramBatch) || !senderBatch->multiple) {
        return invalid("Common datagram execution ports differ from their contract");
    }
    const MediaEdge* activation =
        MediaGraphShapeQuery::singleIncomingEdge(graph, sourceActivation->id);
    const MediaEdge* sourceToSender =
        MediaGraphShapeQuery::singleIncomingEdge(graph, senderPlan->id);
    if (!activation || !sourceToSender ||
        activation->policy != edgePolicies.atomicMetadata ||
        sourceToSender->from.portId != sourcePlan->id ||
        sourceToSender->policy != edgePolicies.atomicMetadata) {
        return invalid("Common datagram execution edges differ from their planner product");
    }
    std::size_t wireEdges = 0;
    for (const MediaNode* materializer : shape.nodes(materializerKind)) {
        const MediaPort* plan = materializer->findInputPort("transport_plan");
        const MediaPort* wire = materializer->findOutputPort("wire_batch");
        const MediaEdge* planEdge = plan
            ? MediaGraphShapeQuery::singleIncomingEdge(graph, plan->id)
            : nullptr;
        if (!MediaGraphShapeQuery::validPort(plan,
                MediaPortDirection::Input, MediaStreamKind::Metadata,
                MediaEdgeKind::Metadata,
                MediaPayloadKind::DatagramTransportPlan) ||
            !MediaGraphShapeQuery::validPort(wire,
                MediaPortDirection::Output, MediaStreamKind::Metadata,
                MediaEdgeKind::ScheduledDatagramBatch,
                MediaPayloadKind::WireDatagramBatch) ||
            !planEdge || planEdge->from.portId != sourcePlan->id ||
            planEdge->policy != edgePolicies.atomicMetadata) {
            return invalid("Protocol materializer transport edge differs from its product");
        }
        const MediaEdge* wireEdge = MediaGraphShapeQuery::singleEdge(
            graph, wire->id, senderBatch->id);
        if (!wireEdge || wireEdge->policy !=
                edgePolicies.synchronizedPacket) {
            return invalid("Protocol materializer is not aggregated by the common pacing sender");
        }
        ++wireEdges;
    }
    return wireEdges == materializerCount
        ? ::media::Status::success()
        : invalid("Common pacing sender has an incomplete protocol materializer set");
}

} // namespace media::ffmpeg::graph
