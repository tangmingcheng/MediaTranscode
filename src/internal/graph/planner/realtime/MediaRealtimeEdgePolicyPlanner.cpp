#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"

namespace media::ffmpeg::graph {
namespace {

MediaEdgePolicy planQueuePolicy(
    std::size_t capacity,
    MediaQueueOverflowPolicy overflowPolicy,
    MediaQueueOrderingPolicy orderingPolicy =
        MediaQueueOrderingPolicy::Timestamp)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::SpscRing;
    policy.queuePolicy.overflowPolicy = overflowPolicy;
    policy.queuePolicy.orderingPolicy = orderingPolicy;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.collectMetrics = true;
    policy.queuePolicy.backpressurePolicy.mode =
        MediaBackpressureMode::Adaptive;
    policy.queuePolicy.backpressurePolicy.lowWatermark = capacity / 2;
    policy.queuePolicy.backpressurePolicy.highWatermark =
        capacity > 0 ? capacity - 1 : 0;
    policy.queuePolicy.backpressurePolicy.criticalWatermark = capacity;
    policy.queuePolicy.backpressurePolicy.realtimePriority = true;
    policy.backpressurePolicy = policy.queuePolicy.backpressurePolicy;
    return policy;
}

MediaEdgePolicy planAtomicOutputPolicy(std::size_t capacity)
{
    MediaEdgePolicy policy = planQueuePolicy(
        capacity, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.preserveOrdering = true;
    return policy;
}

} // namespace

MediaRealtimeEdgePolicySet MediaRealtimeEdgePolicyPlanner::plan(
    const MediaGraphQueueParameters& queues)
{
    MediaRealtimeEdgePolicySet policies;
    policies.metadata = planQueuePolicy(
        queues.metadata, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
    policies.packet = planQueuePolicy(
        queues.packet, MediaQueueOverflowPolicy::DropOldest);
    policies.videoPacket = planQueuePolicy(
        queues.packet, MediaQueueOverflowPolicy::DropNonKeyFrame);
    policies.audioPacket = planQueuePolicy(
        queues.packet, MediaQueueOverflowPolicy::DropOldest);
    policies.synchronizedPacket = planAtomicOutputPolicy(queues.packet);
    policies.audioDriftTransaction = planQueuePolicy(
        queues.frame, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
    policies.videoFrame = planQueuePolicy(
        queues.frame, MediaQueueOverflowPolicy::DropOldest);
    policies.preparedVideoFrame = planAtomicOutputPolicy(queues.frame);
    policies.audioFrame = planQueuePolicy(
        queues.frame, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
    policies.mux = planQueuePolicy(
        queues.mux, MediaQueueOverflowPolicy::DropOldest);
    policies.videoMux = planQueuePolicy(
        queues.mux, MediaQueueOverflowPolicy::DropNonKeyFrame);
    policies.audioMux = planQueuePolicy(
        queues.mux, MediaQueueOverflowPolicy::DropOldest);
    return policies;
}

} // namespace media::ffmpeg::graph
