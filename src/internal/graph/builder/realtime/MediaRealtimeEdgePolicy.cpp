#include "internal/graph/builder/realtime/MediaRealtimeEdgePolicy.h"

namespace media::ffmpeg::graph {

MediaEdgePolicy MediaRealtimeEdgePolicy::make(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::SpscRing;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;
    policy.queuePolicy.orderingPolicy = MediaQueueOrderingPolicy::Timestamp;
    policy.queuePolicy.capacity = options.queueCapacity > 0 ? options.queueCapacity : 8;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.collectMetrics = true;
    policy.queuePolicy.backpressurePolicy.mode = MediaBackpressureMode::Adaptive;
    policy.queuePolicy.backpressurePolicy.lowWatermark = policy.queuePolicy.capacity / 2;
    policy.queuePolicy.backpressurePolicy.highWatermark = options.highWatermark > 0 ? options.highWatermark : policy.queuePolicy.capacity - 2;
    policy.queuePolicy.backpressurePolicy.criticalWatermark = options.criticalWatermark > 0 ? options.criticalWatermark : policy.queuePolicy.capacity;
    policy.queuePolicy.backpressurePolicy.realtimePriority = true;
    policy.backpressurePolicy = policy.queuePolicy.backpressurePolicy;
    return policy;
}

} // namespace media::ffmpeg::graph
