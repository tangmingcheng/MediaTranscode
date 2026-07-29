#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"

namespace media::ffmpeg::graph {

MediaEdgePolicy MediaBlockingEdgePolicyPlanner::planQueue(
    std::size_t capacity) noexcept
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.storageMode = MediaQueueStorageMode::Deque;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy =
        MediaQueueOverflowPolicy::BlockProducer;
    policy.queuePolicy.orderingPolicy = MediaQueueOrderingPolicy::Fifo;
    policy.queuePolicy.preserveOrdering = true;
    policy.queuePolicy.allowFlushControlBypass = true;
    policy.queuePolicy.collectMetrics = true;
    return policy;
}

MediaEdgePolicy MediaBlockingEdgePolicyPlanner::planAtomicOutput(
    std::size_t capacity) noexcept
{
    MediaEdgePolicy policy = planQueue(capacity);
    policy.queuePolicy.storageMode = MediaQueueStorageMode::AtomicPrepared;
    return policy;
}

MediaRealtimeEdgePolicySet MediaBlockingEdgePolicyPlanner::plan(
    const MediaGraphQueueParameters& queues) noexcept
{
    MediaRealtimeEdgePolicySet policies;
    policies.metadata = planQueue(queues.metadata);
    policies.packet = planQueue(queues.packet);
    policies.videoPacket = planQueue(queues.packet);
    policies.audioPacket = planQueue(queues.packet);
    policies.synchronizedPacket = planQueue(queues.packet);
    policies.videoFrame = planQueue(queues.frame);
    policies.audioFrame = planQueue(queues.frame);
    policies.mux = planQueue(queues.mux);
    policies.videoMux = planQueue(queues.mux);
    policies.audioMux = planQueue(queues.mux);
    policies.atomicMetadata = planAtomicOutput(queues.metadata);
    policies.atomicVideoPacket = planAtomicOutput(queues.packet);
    policies.atomicAudioPacket = planAtomicOutput(queues.packet);
    policies.audioDriftTransaction = planAtomicOutput(queues.frame);
    policies.preparedVideoFrame = planAtomicOutput(queues.frame);
    return policies;
}

} // namespace media::ffmpeg::graph
