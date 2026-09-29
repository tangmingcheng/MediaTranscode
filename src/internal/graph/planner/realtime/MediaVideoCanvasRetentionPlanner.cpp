#include "internal/graph/planner/realtime/MediaVideoCanvasRetentionPlanner.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoCanvasRetentionPlan> MediaVideoCanvasRetentionPlanner::plan(
    const MediaGraph& graph, MediaNodeId aggregate,
    const MediaPreparedEncoderEmissionEnvelope& encoder)
{
    using Result = ::media::Result<MediaVideoCanvasRetentionPlan>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    const auto* producer = graph.findNode(aggregate);
    if (!producer || producer->kind != MediaNodeKind::AvContinuousAggregate ||
        encoder.authority.empty() || encoder.backend.empty() ||
        !encoder.accessUnitsPerSecondNumerator || !encoder.accessUnitsPerSecondDenominator ||
        !encoder.maximumAccessUnitPayloadBytes)
        return invalid("Canvas retention requires the aggregate and prepared encoder facts");
    const MediaEdge* output = nullptr;
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId != aggregate || edge.streamKind != MediaStreamKind::Video ||
            edge.payloadKind != MediaPayloadKind::Frame) continue;
        if (output) return invalid("Canvas retention rejects branched video output");
        output = &edge;
    }
    if (!output || !output->isValid() || !MediaAtomicOutputPolicyContract::accepts(output->policy))
        return invalid("Canvas retention requires its bounded atomic video edge");
    const auto* consumer = graph.findNode(output->to.nodeId);
    const auto* sourcePort = graph.findPort(output->from.portId);
    const auto* targetPort = graph.findPort(output->to.portId);
    if (!consumer || consumer->kind != MediaNodeKind::VideoEncode ||
        !sourcePort || sourcePort->name != "video" ||
        !targetPort || targetPort->name != "frame")
        return invalid("Canvas retention requires a direct aggregate-to-encoder video path");
    for (const auto& edge : graph.edges()) {
        if (edge.to.nodeId == consumer->id && edge.payloadKind == MediaPayloadKind::Frame &&
            edge.id != output->id)
            return invalid("Canvas encoder cannot have another frame producer");
    }
    // These are physical member cardinalities, not latency-derived spare slots:
    // aggregate::m_pending, VideoEncodeNode::pendingFrame, canvas::black_.
    constexpr std::uint64_t aggregatePending = 1;
    constexpr std::uint64_t encoderPending = 1;
    constexpr std::uint64_t fixedBlackSurface = 1;
    auto pool = MediaEncoderHardwareFramesPoolPlanner::plan({
        static_cast<std::uint64_t>(output->policy.queuePolicy.capacity),
        aggregatePending + encoderPending, encoder.maximumEncoderRetainedFrames,
        fixedBlackSurface, "final-direct-canvas-edge+aggregate-pending+encoder-pending+" +
            encoder.authority + "+fixed-black-surface"});
    if (!pool) return Result::failure(pool.error());
    if (pool.value().initialPoolSurfaces > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        pool.value().initialPoolSurfaces > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return invalid("Canvas pool retention exceeds platform allocation counts");
    const auto circulating = static_cast<std::size_t>(pool.value().initialPoolSurfaces - fixedBlackSurface);
    return Result::success({std::move(pool).value(), circulating, circulating});
}
} // namespace media::ffmpeg::graph
