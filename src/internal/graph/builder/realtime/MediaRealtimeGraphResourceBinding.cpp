#include "internal/graph/builder/realtime/MediaRealtimeGraphResourceBinding.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeGraphResourceBinding::apply(
    MediaGraph& graph, MediaNodeId codecResolver, const MediaFinalGraphResourceLedger& ledger)
{
    const auto* node = graph.findNode(codecResolver);
    if (!node || node->kind != MediaNodeKind::CodecResolver ||
        !ledger.payloadCreditPlan.isCompleteAndValid() ||
        !graph.setPayloadCreditPlan(ledger.payloadCreditPlan))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Realtime graph requires its output codec resolver and complete payload credit plan"));
    const auto set = [&](const char* key, const std::string& value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(graph,
            "MediaRealtimeGraphResourceBinding", codecResolver, key, value);
    };
    if (auto status = set("resource.graph_payload_reserved_bytes",
            std::to_string(ledger.admittedGraphPayloadAndReservedStorageBytes)); !status) return status;
    if (auto status = set("resource.observed_external_allocation",
            ledger.outOfScopeAuthorities.empty() ? "0" : "1"); !status) return status;
    if (ledger.encoderFramesPool) {
        if (auto status = set("encoder.hardware_frames.initial_pool_surfaces",
                std::to_string(ledger.encoderFramesPool->initialPoolSurfaces)); !status) return status;
        if (auto status = set("encoder.hardware_frames.pool_authority",
                ledger.encoderFramesPool->authority); !status) return status;
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
