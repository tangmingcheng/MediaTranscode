#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"

#include <utility>

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
    policy.queuePolicy.storageMode = MediaQueueStorageMode::Deque;
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
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.storageMode = MediaQueueStorageMode::AtomicPrepared;
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
    policies.synchronizedPacket = planQueuePolicy(
        queues.packet, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
    policies.audioDriftTransaction = planAtomicOutputPolicy(queues.frame);
    policies.videoFrame = planQueuePolicy(
        queues.frame, MediaQueueOverflowPolicy::DropOldest);
    policies.synchronizedVideoFrame = planQueuePolicy(
        queues.frame, MediaQueueOverflowPolicy::BlockProducer,
        MediaQueueOrderingPolicy::Fifo);
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
    policies.atomicMetadata = planAtomicOutputPolicy(queues.metadata);
    policies.atomicVideoPacket = planAtomicOutputPolicy(queues.packet);
    policies.atomicAudioPacket = planAtomicOutputPolicy(queues.packet);
    return policies;
}

::media::Result<MediaRealtimeEdgePolicySet>
MediaRealtimeEdgePolicyPlanner::planWithSynchronizedPacketMemoryBudget(
    const MediaGraphQueueParameters& queues,
    std::uint64_t maximumBytes,
    std::size_t maximumBuffers)
{
    if (maximumBytes == 0 || maximumBuffers == 0 ||
        maximumBuffers != queues.packet) {
        return ::media::Result<MediaRealtimeEdgePolicySet>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized packet memory policy requires exact queue and byte bounds"));
    }
    MediaRealtimeEdgePolicySet policies = plan(queues);
    auto& memory = policies.synchronizedPacket.bufferPolicy.memoryBudget;
    memory.maxBytes = maximumBytes;
    memory.softLimitBytes = maximumBytes;
    memory.reservedBytes = 0;
    memory.maxBuffers = maximumBuffers;
    memory.preallocatedBuffers = 0;
    memory.enforceHardLimit = true;
    memory.allowDynamicGrowth = false;
    return ::media::Result<MediaRealtimeEdgePolicySet>::success(
        std::move(policies));
}

} // namespace media::ffmpeg::graph
