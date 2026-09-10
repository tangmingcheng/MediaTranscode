#include "internal/graph/planner/realtime/MediaRealtimeOutputSourceRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>

namespace media::ffmpeg::graph {
::media::Result<MediaGraphPayloadRetentionGrowth>
MediaRealtimeOutputSourceRetentionPlanner::plan(
    const MediaGraph& graph,
    std::span<const MediaNodeId> outputNodes,
    MediaNodeId sourceFanout,
    const MediaRealtimeGraphResourceLedgerPlan& planning,
    const MediaFinalGraphResourceLedger& outputResources)
{
    using Result = ::media::Result<MediaGraphPayloadRetentionGrowth>;
    const auto* fanout = graph.findNode(sourceFanout);
    if (!fanout || fanout->kind != MediaNodeKind::VideoOutputFanout ||
        !graph.payloadCreditPlan() || outputNodes.empty()) return Result::failure(
        ::media::ErrorInfo::notInitialized("source retention requires a shared frame distributor and new output nodes"));
    MediaNodeId producer = MediaNodeId::invalid();
    for (const auto& edge : graph.edges()) {
        if (edge.to.nodeId != sourceFanout || edge.payloadKind != MediaPayloadKind::Frame) continue;
        if (producer.isValid()) return Result::failure(::media::ErrorInfo::invalidArgument(
            "shared frame distributor has multiple payload owners"));
        producer = edge.from.nodeId;
    }
    const auto& strategies = graph.payloadCreditPlan()->producers;
    const auto strategy = std::find_if(strategies.begin(), strategies.end(), [&](const auto& value) {
        return value.nodeId == producer && value.payloadKind == MediaPayloadKind::Frame &&
            value.streamKind == MediaStreamKind::Video;
    });
    if (strategy == strategies.end()) return Result::failure(::media::ErrorInfo::notInitialized(
        "shared frame distributor has no admitted upstream producer"));
    std::uint64_t references = 0;
    for (const auto& node : graph.nodes()) {
        if (std::find(outputNodes.begin(), outputNodes.end(), node.id) == outputNodes.end()) continue;
        bool consumesFrame = false;
        for (const auto& edge : graph.edges()) {
            if (edge.to.nodeId != node.id || edge.payloadKind != MediaPayloadKind::Frame ||
                edge.streamKind != MediaStreamKind::Video) continue;
            consumesFrame = true;
            auto next = MediaCheckedArithmetic::add(references, edge.policy.queuePolicy.capacity,
                "new output queued source-frame references");
            if (!next) return Result::failure(next.error());
            references = next.value();
        }
        if (consumesFrame) {
            // One currently popped input per worker, separate from its queue.
            auto next = MediaCheckedArithmetic::add(references, 1,
                "new output active source-frame input slot");
            if (!next) return Result::failure(next.error());
            references = next.value();
        }
    }
    auto pending = MediaCheckedArithmetic::add(references,
        outputResources.videoPipelinePendingSurfaces, "new output typed pending surface slots");
    auto retained = pending ? MediaCheckedArithmetic::add(pending.value(),
        planning.maximumEncoderRetainedFrames, "new output prepared encoder retention") : pending;
    if (!retained || retained.value() == 0) return Result::failure(
        !retained ? retained.error() : ::media::ErrorInfo::notInitialized("new output has no frame retention contract"));
    auto bytes = MediaCheckedArithmetic::multiply(retained.value(), strategy->maximumReservedBytes(),
        "new output shared source allocation residence");
    if (!bytes) return Result::failure(bytes.error());
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::GraphBuild,
        "source_retention_growth producer=" + std::to_string(producer.value) +
        " engine_bytes=" + std::to_string(bytes.value()) +
        " objects=" + std::to_string(retained.value()) +
        " authority=new-frame-queues+active-input-slots+typed-pipeline-pending+encoder-readback");
    return Result::success({producer, bytes.value(), retained.value()});
}
} // namespace media::ffmpeg::graph
