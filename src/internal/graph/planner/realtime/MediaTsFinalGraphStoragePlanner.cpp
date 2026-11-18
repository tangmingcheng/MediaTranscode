#include "internal/graph/planner/realtime/MediaTsFinalGraphStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaNetworkStorageWakeupPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"
#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <new>
#include <limits>
#include "internal/graph/planner/realtime/MediaTsSeriesOwnershipPlanner.h"

namespace media::ffmpeg::graph {
::media::Result<MediaDatagramTransportPlanTemplate> MediaTsFinalGraphStoragePlanner::plan(
    const MediaGraph& graph, MediaNodeId source,
    const MediaDatagramTransportPlanTemplate& base) try
{
    using Result = ::media::Result<MediaDatagramTransportPlanTemplate>;
    if (!base.encode().deployment.resources.mpegTsProducer) return Result::success(base);
    auto topology = MediaNetworkStorageWakeupPlanner::plan(graph, source);
    if (!topology) return Result::failure(topology.error());
    if (topology.value().sessionKey.value() != base.sessionKey()) return Result::failure(
        ::media::ErrorInfo::invalidArgument("Final TS storage belongs to another session"));
    std::optional<MediaTsMuxQueueStoragePlan> storage;
    std::optional<MediaNodeId> muxOwner;
    std::size_t maximumPackets = 0;
    std::size_t maximumDatagrams = 0;
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId != source || edge.payloadKind != MediaPayloadKind::DatagramTransportPlan) continue;
        const auto* node = graph.findNode(edge.to.nodeId);
        if (!node || node->kind != MediaNodeKind::ProjectMpegTsPlanSource) continue;
        if (storage) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Final TS storage requires one mux per transport generation"));
        std::vector<MediaNodeId> muxes;
        for (const auto& forwarding : graph.edges()) {
            if (forwarding.from.nodeId != node->id ||
                forwarding.payloadKind != MediaPayloadKind::ProjectMpegTsRuntimePlan) continue;
            // The runtime plan also feeds AU adapters, materializers and SDP
            // publishers. Queue ownership belongs to a TS batch producer only.
            // The wakeup planner above validates every typed forwarding edge
            // and every producer-to-transport-consumer batch edge.
            const bool producesProtocolBatch = std::any_of(graph.edges().begin(), graph.edges().end(),
                [&](const MediaEdge& batch) {
                    return batch.from.nodeId == forwarding.to.nodeId &&
                        batch.payloadKind == MediaPayloadKind::MpegTsProtocolDatagramBatch;
                });
            if (!producesProtocolBatch) continue;
            const auto* mux = graph.findNode(forwarding.to.nodeId);
            if (!mux || mux->kind != MediaNodeKind::FileMux ||
                std::find(topology.value().producerTargets.begin(), topology.value().producerTargets.end(), mux->id) ==
                    topology.value().producerTargets.end())
                return Result::failure(::media::ErrorInfo::invalidArgument("Final TS queue has an unbound mux owner"));
            if (std::find(muxes.begin(), muxes.end(), mux->id) == muxes.end()) muxes.push_back(mux->id);
        }
        if (muxes.size() != 1) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Final TS queue requires exactly one typed mux owner"));
        auto protocol = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(*node);
        if (!protocol) return Result::failure(protocol.error());
        auto queue = MediaTsMuxQueueStoragePlanner::plan(protocol.value().outputPlan.emission.maximumQueuedBytes());
        if (!queue) return Result::failure(queue.error());
        storage = queue.value();
        muxOwner = muxes.front();
        const auto& bounds = protocol.value().outputPlan.seriesBounds;
        const auto maintenancePackets = (std::max)({bounds.maximumPatPackets,
            bounds.maximumPmtPackets, bounds.maximumPcrPackets});
        maximumPackets = (std::max)(bounds.video.maximumPackets, maintenancePackets);
        // Maintenance cannot produce more datagrams than its packet count.
        maximumDatagrams = (std::max)(bounds.video.maximumDatagrams, maintenancePackets);
        if (bounds.audio) {
            maximumPackets = (std::max)(maximumPackets, bounds.audio->maximumPackets);
            maximumDatagrams = (std::max)(maximumDatagrams, bounds.audio->maximumDatagrams);
        }
    }
    if (!storage) return Result::failure(::media::ErrorInfo::notInitialized(
        "Final TS storage requires a connected protocol emission plan"));
    auto deployment = base.encode().deployment;
    deployment.resources.mpegTsProducer->muxQueueStorage = *storage;
    deployment.resources.mpegTsProducer->seriesStorage.reset();
    auto network = MediaRealtimeNetworkResourceLedgerPlanner::plan(
        deployment.latency, deployment.observation, base.encode().wireTraffic,
        deployment.localPorts.addressFamily, deployment.resources.protocolLayout,
        deployment.resources.rawRtpProducers, deployment.resources.mpegTsProducer);
    if (!network) return Result::failure(network.error());
    if (!muxOwner || network.value().maximumBacklogDatagrams > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::invalidArgument("TS final ownership facts are not representable"));
    auto ownership = MediaTsSeriesOwnershipPlanner::plan(graph, source, *muxOwner, storage->slots,
        static_cast<std::size_t>(network.value().maximumBacklogDatagrams));
    if (!ownership) return Result::failure(ownership.error());
    auto live = ownership.value().total();
    if (!live) return Result::failure(live.error());
    auto views = ownership.value().prefixViews();
    if (!views) return Result::failure(views.error());
    auto series = MediaTsSeriesStoragePlanner::plan(maximumPackets, maximumDatagrams, live.value(), views.value());
    if (!series) return Result::failure(series.error());
    deployment.resources.mpegTsProducer->seriesStorage = series.value();
    network = MediaRealtimeNetworkResourceLedgerPlanner::plan(
        deployment.latency, deployment.observation, base.encode().wireTraffic,
        deployment.localPorts.addressFamily, deployment.resources.protocolLayout,
        deployment.resources.rawRtpProducers, deployment.resources.mpegTsProducer);
    if (!network) return Result::failure(network.error());
    auto total = MediaCheckedArithmetic::add(network.value().admittedNetworkBytes,
        network.value().admittedSocketBytes, "final TS network storage admission");
    if (!total) return Result::failure(total.error());
    deployment.resources.maximumNetworkMemoryBytes = total.value();
    auto envelope = MediaRealtimeDeploymentEnvelope::decode(std::move(deployment));
    if (!envelope) return Result::failure(envelope.error());
    return MediaDatagramTransportPlanTemplate::create(base.sessionKey(), envelope.value(),
        base.remoteEndpoints(), base.encode().wireTraffic);
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
        ::media::ErrorInfo::allocationFailed("Final TS storage planning"));
}

::media::Status MediaTsFinalGraphStoragePlanner::apply(MediaGraph& graph,
    std::span<const MediaNodeId> scope)
{
    for (const auto& node : graph.nodes()) {
        if (node.kind != MediaNodeKind::DatagramTransportPlanSource ||
            (!scope.empty() && std::find(scope.begin(), scope.end(), node.id) == scope.end())) continue;
        auto base = MediaDatagramTransportPlanSourceNodePlanCodec::decode(node);
        if (!base) return ::media::Status::failure(base.error());
        auto final = plan(graph, node.id, base.value());
        if (!final) return ::media::Status::failure(final.error());
        if (auto status = MediaDatagramTransportPlanSourceNodePlanCodec::apply(graph, node.id, final.value()); !status)
            return status;
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
