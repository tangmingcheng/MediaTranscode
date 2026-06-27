#include "internal/graph/runtime/MediaBackpressureController.h"

#include "internal/graph/runtime/channel/MediaChannel.h"

namespace media::ffmpeg::graph {

MediaBackpressureReport MediaBackpressureController::inspect(const MediaGraphExecutionContext& context)
{
    MediaBackpressureReport report;

    for (const MediaChannel* channel : context.channels().channels()) {
        if (!channel) {
            continue;
        }

        const std::size_t size = channel->size();
        const std::size_t capacity = channel->capacity();
        const auto& policy = channel->policy().queuePolicy;
        const auto& pressure = policy.backpressurePolicy;

        MediaBackpressureDecision decision;
        decision.edgeId = channel->edgeId();
        decision.queueSize = size;
        decision.capacity = capacity;

        if (capacity > 0 && size >= capacity) {
            decision.kind = MediaBackpressureDecisionKind::QueueFull;
            decision.message = "queue is full";
        } else if (pressure.criticalWatermark > 0 && size >= pressure.criticalWatermark) {
            decision.kind = MediaBackpressureDecisionKind::AboveCriticalWatermark;
            decision.message = "queue reached critical watermark";
        } else if (pressure.highWatermark > 0 && size >= pressure.highWatermark) {
            decision.kind = MediaBackpressureDecisionKind::AboveHighWatermark;
            decision.message = "queue reached high watermark";
        } else {
            decision.kind = MediaBackpressureDecisionKind::Healthy;
            decision.message = "queue healthy";
        }

        report.decisions.push_back(std::move(decision));
    }

    return report;
}

} // namespace media::ffmpeg::graph
