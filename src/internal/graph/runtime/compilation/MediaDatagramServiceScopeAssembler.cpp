#include "internal/graph/runtime/compilation/MediaDatagramServiceScopeAssembler.h"

#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/planner/realtime/MediaDatagramServiceScopePlanner.h"

namespace media::ffmpeg::graph {

::media::Result<std::vector<MediaDatagramServiceScopeBinding>>
MediaDatagramServiceScopeAssembler::assemble(
    const MediaGraph& graph,
    const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>& authority)
{
    using Result = ::media::Result<std::vector<MediaDatagramServiceScopeBinding>>;
    std::vector<MediaDatagramServiceScopeBinding> bindings;
    for (const auto& sender : graph.nodes()) {
        if (sender.kind != MediaNodeKind::ScheduledDatagramSender) continue;
        const auto* port = graph.findInputPort(sender.id, "plan");
        if (!port || !authority) return Result::failure(::media::ErrorInfo::notInitialized(
            "Datagram scope assembly requires the sender's plan port and clock authority"));
        const MediaNode* source = nullptr;
        for (const auto& edge : graph.edges()) {
            if (edge.to.nodeId != sender.id || edge.to.portId != port->id) continue;
            if (source) return Result::failure(::media::ErrorInfo::invalidArgument(
                "Datagram sender has multiple transport planning authorities"));
            source = graph.findNode(edge.from.nodeId);
            if (!source || source->kind != MediaNodeKind::DatagramTransportPlanSource)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Datagram scope assembly requires its actual transport plan producer"));
        }
        if (!source) return Result::failure(::media::ErrorInfo::notInitialized(
            "Datagram sender has no connected transport plan producer"));
        auto transport = MediaDatagramTransportPlanSourceNodePlanCodec::decode(*source);
        if (!transport) return Result::failure(transport.error());
        if (transport.value().sessionKey() != authority->sessionKey().value())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Datagram transport and injected clock authority belong to different sessions"));
        auto planned = MediaDatagramServiceScopePlanner::plan(transport.value());
        if (!planned) return Result::failure(planned.error());
        std::shared_ptr<MediaDatagramServiceScopeArbiter> arbiter;
        for (const auto& binding : bindings) {
            const auto& existing = binding.arbiter->contract();
            if (existing.kind != planned.value().kind || existing.scopeId != planned.value().scopeId)
                continue;
            if (existing != planned.value()) return Result::failure(::media::ErrorInfo::invalidArgument(
                "One datagram service scope has conflicting capacity or coverage authorities"));
            arbiter = binding.arbiter;
            break;
        }
        if (!arbiter) {
            auto created = MediaDatagramServiceScopeArbiter::create(std::move(planned).value(), authority);
            if (!created) return Result::failure(created.error());
            arbiter = std::move(created).value();
        }
        bindings.push_back({sender.id, std::move(arbiter)});
    }
    return Result::success(std::move(bindings));
}

} // namespace media::ffmpeg::graph
