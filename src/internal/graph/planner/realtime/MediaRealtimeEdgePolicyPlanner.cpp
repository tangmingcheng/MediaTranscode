#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"

#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

#include <limits>
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

::media::Result<MediaEdgePolicy> MediaRealtimeEdgePolicyPlanner::planCanonicalAudioInput(
    MediaBranchMode mode, const MediaEdgePolicy& outputPacket,
    const std::optional<MediaEdgePolicy>& sourceRelease)
{
    using Result = ::media::Result<MediaEdgePolicy>;
    if (mode == MediaBranchMode::CopyPacket) {
        if (!sourceRelease || !MediaAtomicOutputPolicyContract::accepts(*sourceRelease))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Packet-copy scheduler input requires its source atomic release product"));
        return Result::success(*sourceRelease);
    }
    if (mode != MediaBranchMode::TranscodeFrame || sourceRelease)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Encoded scheduler input cannot carry a source packet release product"));
    return Result::success(outputPacket);
}

::media::Result<MediaRealtimeEdgePolicySet>
MediaRealtimeEdgePolicyPlanner::planSynchronizedOutput(
    const MediaRealtimeGraphResourceLedgerPlan& resources)
{
    using Result = ::media::Result<MediaRealtimeEdgePolicySet>;
    if (auto status = MediaRealtimeGraphResourceLedgerPlanner::validate(resources); !status)
        return Result::failure(status.error());
    if (!resources.media.audioBytes)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Synchronized output requires prepared audio retention"));
    auto bytes = MediaCheckedArithmetic::add(resources.media.videoBytes,
        *resources.media.audioBytes, "prepared output packet retention");
    if (!bytes) return Result::failure(bytes.error());
    return planWithSynchronizedPacketMemoryBudget(
        resources.queues, bytes.value(), resources.queues.packet);
}

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

::media::Result<MediaRealtimeEdgePolicySet>
MediaRealtimeEdgePolicyPlanner::planWithAvStartupRelease(
    const MediaGraphQueueParameters& queues,
    std::uint64_t maximumBytes,
    std::size_t maximumBuffers,
    std::size_t maximumVideoReleaseUnits,
    std::size_t maximumAudioReleaseUnits)
{
    if (maximumVideoReleaseUnits == 0 || maximumAudioReleaseUnits == 0) {
        return ::media::Result<MediaRealtimeEdgePolicySet>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup release requires explicit batch bounds"));
    }
    auto policies = planWithSynchronizedPacketMemoryBudget(
        queues, maximumBytes, maximumBuffers);
    if (!policies) return policies;
    // Only release-extractor outputs reserve the entire input startup batch.
    // Encoded and scheduled output edges retain their wire-residence bounds.
    policies.value().startupVideoRelease =
        planAtomicOutputPolicy(maximumVideoReleaseUnits);
    policies.value().startupAudioRelease =
        planAtomicOutputPolicy(maximumAudioReleaseUnits);
    return policies;
}

::media::Result<MediaRealtimeEdgePolicySet> MediaRealtimeEdgePolicyPlanner::planSynchronizedSource(
    const MediaGraphQueueParameters& queues,
    const MediaAvSyncStartupPolicy& startup,
    MediaTranscodeStreamSet members)
{
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        !startup.videoByteCapacity ||
        hasAudio != startup.audioByteCapacity.has_value() ||
        !startup.videoCapacity ||
        hasAudio != startup.audioCapacity.has_value() ||
        *startup.videoByteCapacity == 0 ||
        *startup.videoCapacity == 0 ||
        (hasAudio && (*startup.audioByteCapacity == 0 || *startup.audioCapacity == 0)) ||
        queues.packet == 0 ||
        *startup.videoByteCapacity >
            (std::numeric_limits<std::uint64_t>::max)() -
                (hasAudio ? *startup.audioByteCapacity : 0)) {
        return ::media::Result<MediaRealtimeEdgePolicySet>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V edge byte capacity is incomplete or not representable"));
    }
    const auto maximumBytes =
        *startup.videoByteCapacity +
        (hasAudio ? *startup.audioByteCapacity : 0);
    if (!hasAudio) {
        auto policies = planWithSynchronizedPacketMemoryBudget(queues, maximumBytes, queues.packet);
        if (!policies) return policies;
        policies.value().startupVideoRelease = planAtomicOutputPolicy(*startup.videoCapacity);
        // Other edge recipes are not installed: this source has no audio ports.
        return policies;
    }
    return MediaRealtimeEdgePolicyPlanner::
        planWithAvStartupRelease(
            queues, maximumBytes, queues.packet,
            *startup.videoCapacity,
            *startup.audioCapacity);
}

} // namespace media::ffmpeg::graph
