#include "internal/graph/planner/realtime/MediaTsSeriesOwnershipPlanner.h"
#include "internal/graph/planner/realtime/MediaNetworkStorageWakeupPlanner.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"
#include <array>
#include <initializer_list>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
const MediaEdge* typedInput(const MediaGraph& graph, const MediaNode& node,
    const char* name, MediaPayloadKind payload)
{
    const auto* port = node.findInputPort(name);
    const auto* edge = port ? MediaGraphShapeQuery::singleIncomingEdge(graph, port->id) : nullptr;
    return edge && edge->payloadKind == payload ? edge : nullptr;
}

bool exactPacketInputs(const MediaGraph& graph, const MediaNode& node,
    std::initializer_list<const char*> names)
{
    std::size_t count = 0;
    for (const auto& edge : graph.edges())
        if (edge.to.nodeId == node.id && edge.edgeKind == MediaEdgeKind::EncodedPacket) ++count;
    if (count != names.size()) return false;
    for (const auto* name : names) {
        const auto* port = node.findInputPort(name);
        const auto* edge = port ? MediaGraphShapeQuery::singleIncomingEdge(graph, port->id) : nullptr;
        if (!edge || edge->edgeKind != MediaEdgeKind::EncodedPacket) return false;
    }
    return true;
}

bool fromOutput(const MediaEdge& edge, const MediaNode& node, const char* name)
{
    const auto* port = node.findOutputPort(name);
    return port && edge.from.nodeId == node.id && edge.from.portId == port->id;
}

::media::Status validateTerminalSource(const MediaGraph& graph, MediaNodeId transport, const MediaNode& mux)
{
    const auto invalid = [] { return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "TS mux requires one scheduled packet input with a shared plan and aggregate terminal source")); };
    if (!exactPacketInputs(graph, mux, {"packet"})) return invalid();
    const auto* packet = typedInput(graph, mux, "packet", MediaPayloadKind::TsAccessUnit);
    const auto* muxPlan = typedInput(graph, mux, "plan", MediaPayloadKind::ProjectMpegTsRuntimePlan);
    const auto* adapter = packet ? graph.findNode(packet->from.nodeId) : nullptr;
    const auto* plan = muxPlan ? graph.findNode(muxPlan->from.nodeId) : nullptr;
    if (!adapter || adapter->kind != MediaNodeKind::ScheduledTsAccessUnitAdapter ||
        !plan || plan->kind != MediaNodeKind::ProjectMpegTsPlanSource ||
        !fromOutput(*packet, *adapter, "packet") || !fromOutput(*muxPlan, *plan, "plan") ||
        !exactPacketInputs(graph, *adapter, {"scheduled"})) return invalid();
    const auto* adapterPlan = typedInput(graph, *adapter, "plan", MediaPayloadKind::ProjectMpegTsRuntimePlan);
    const auto* transportPlan = typedInput(graph, *plan, "transport_plan", MediaPayloadKind::DatagramTransportPlan);
    const auto* scheduled = typedInput(graph, *adapter, "scheduled", MediaPayloadKind::Packet);
    if (!adapterPlan || !fromOutput(*adapterPlan, *plan, "plan") ||
        !transportPlan || transportPlan->from.nodeId != transport || !scheduled) return invalid();
    auto decoded = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(*plan);
    if (!decoded) return ::media::Status::failure(decoded.error());
    if (adapter->options.value("scheduled_ts_adapter.session") != decoded.value().sessionKey.value() ||
        !MediaGraphShapeQuery::matchesStreamSetOption(adapter->options,
            "scheduled_ts_adapter.stream_set", decoded.value().streamSet)) return invalid();
    const auto* producer = graph.findNode(scheduled->from.nodeId);
    if (!producer) return invalid();
    // These are the existing scheduler terminal contracts, independent of
    // codec, hardware, input source count, and composition layout.
    if (decoded.value().streamSet == MediaTranscodeStreamSet::VideoOnly) {
        if (producer->kind != MediaNodeKind::VideoOutputScheduler ||
            producer->options.value("protocol_output.session") != decoded.value().sessionKey.value() ||
            !fromOutput(*scheduled, *producer, "scheduled_video") ||
            !exactPacketInputs(graph, *producer, {"video"}) ||
            !typedInput(graph, *producer, "video", MediaPayloadKind::Packet)) return invalid();
    } else if (decoded.value().streamSet == MediaTranscodeStreamSet::AudioVideo) {
        if (producer->kind != MediaNodeKind::ScheduledOutputRouter ||
            producer->options.value("scheduled_output_router.mode") != "serialized_av" ||
            !fromOutput(*scheduled, *producer, "serialized") ||
            !exactPacketInputs(graph, *producer, {"scheduled"})) return invalid();
        const auto* routed = typedInput(graph, *producer, "scheduled", MediaPayloadKind::Packet);
        const auto* scheduler = routed ? graph.findNode(routed->from.nodeId) : nullptr;
        if (!scheduler || scheduler->kind != MediaNodeKind::AvOutputScheduler ||
            !fromOutput(*routed, *scheduler, "scheduled") ||
            scheduler->options.value("av_scheduler.sync_group") != decoded.value().sessionKey.value() ||
            !exactPacketInputs(graph, *scheduler, {"video", "audio"}) ||
            !typedInput(graph, *scheduler, "video", MediaPayloadKind::Packet) ||
            !typedInput(graph, *scheduler, "audio", MediaPayloadKind::Packet)) return invalid();
    } else return invalid();
    return ::media::Status::success();
}

template <std::size_t N>
::media::Result<std::size_t> sumOwnershipSlots(const std::array<std::size_t, N>& counts)
{
    using Result = ::media::Result<std::size_t>;
    std::size_t result = 0;
    for (const auto count : counts) {
        if (!count || count > (std::numeric_limits<std::size_t>::max)() - result)
            return Result::failure(::media::ErrorInfo::invalidArgument("TS ownership slots are absent or overflow"));
        result += count;
    }
    return Result::success(result);
}
}
::media::Result<std::size_t> MediaTsSeriesOwnershipPlan::total() const
{
    return sumOwnershipSlots(std::array{muxQueuedSeries, muxPendingMediaSeries,
            muxMaintenanceWorkingSeries, muxPublicationSeries, protocolEdgeSeries,
            materializerCurrentSeries, globallyReservedSeries, senderCommitRetirementSeries});
}
::media::Result<std::size_t> MediaTsSeriesOwnershipPlan::prefixViews() const
{
    // The pending AU and maintenance preparation retain root series, not views.
    // Each downstream holder may retain one view in addition to its series.
    return sumOwnershipSlots(std::array{muxQueuedSeries, muxPublicationSeries,
        protocolEdgeSeries, materializerCurrentSeries, globallyReservedSeries,
        senderCommitRetirementSeries});
}
::media::Result<MediaTsSeriesOwnershipPlan> MediaTsSeriesOwnershipPlanner::plan(
    const MediaGraph& graph, MediaNodeId source, MediaNodeId mux,
    std::size_t queued, std::size_t outstanding)
{
    using Result = ::media::Result<MediaTsSeriesOwnershipPlan>;
    const auto invalid = [] { return Result::failure(::media::ErrorInfo::invalidArgument(
        "TS series ownership requires a closed mux/materializer/sender topology")); };
    auto typedTopology = MediaNetworkStorageWakeupPlanner::plan(graph, source);
    if (!typedTopology) return Result::failure(typedTopology.error());
    const auto* muxNode = graph.findNode(mux);
    if (!muxNode || muxNode->kind != MediaNodeKind::FileMux) return invalid();
    auto terminalSource = validateTerminalSource(graph, source, *muxNode);
    if (!terminalSource) return Result::failure(terminalSource.error());
    const MediaEdge* protocol = nullptr;
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId != mux || edge.payloadKind != MediaPayloadKind::MpegTsProtocolDatagramBatch) continue;
        if (protocol || !edge.policy.queuePolicy.isBoundedQueue() ||
            edge.policy.queuePolicy.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer) return invalid();
        protocol = &edge;
    }
    if (!protocol) return invalid();
    const auto* materializer = graph.findNode(protocol->to.nodeId);
    if (!materializer || materializer->kind != MediaNodeKind::MpegTsDatagramMaterializer) return invalid();
    const MediaEdge* wire = nullptr;
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId != materializer->id || edge.payloadKind != MediaPayloadKind::WireDatagramBatch) continue;
        if (wire || !edge.policy.queuePolicy.isBoundedQueue() ||
            edge.policy.queuePolicy.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer) return invalid();
        wire = &edge;
    }
    if (!wire) return invalid();
    const auto* sender = graph.findNode(wire->to.nodeId);
    if (!sender || sender->kind != MediaNodeKind::ScheduledDatagramSender) return invalid();
    for (const auto target : {materializer->id, sender->id}) {
        std::size_t inputs = 0;
        for (const auto& edge : graph.edges()) {
            if (edge.to.nodeId != target) continue;
            if (edge.payloadKind == MediaPayloadKind::DatagramTransportPlan) {
                if (edge.from.nodeId != source) return invalid();
                ++inputs;
            }
            if (edge.payloadKind == MediaPayloadKind::WireDatagramBatch && &edge != wire) return invalid();
            if (edge.payloadKind == MediaPayloadKind::MpegTsProtocolDatagramBatch && &edge != protocol) return invalid();
        }
        if (inputs != 1) return invalid();
    }
    // Structural slots of the verified node kinds: one pending emission, one
    // sequential maintenance cursor, one publication, one current protocol.
    // Prepared/current and publication/front are aliases. Reserved wire owners
    // share the global bound; after global commit one sender call can still own
    // the protocol model until its protocol commit returns.
    MediaTsSeriesOwnershipPlan product{queued, 1,
        MediaTsTransportPacketizer::maintenancePreparationSlots(), 1,
        protocol->policy.queuePolicy.capacity, 1, outstanding, 1};
    auto total = product.total();
    return total ? Result::success(product) : Result::failure(total.error());
}
} // namespace media::ffmpeg::graph
