#include "internal/graph/planner/realtime/MediaCompositionAdmissionMaterializer.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }

::media::Status validate(const MediaPreparedRtpStorageSourceSnapshot& held,
    const MediaCompositionSourceCapacityCandidate& candidate,
    const MediaCompositionSourcePlanningProducts& source,
    const MediaPreparedSourceCandidateCosts& costs)
{
    if (!held.plan || !held.snapshot.owner || !source.planningResource || !source.selectedCapacity)
        return invalid("Admission materialization requires a complete source workspace");
    const bool audio = held.plan->audio.has_value();
    const auto& capacity = candidate.capacity;
    if (!held.plan->storageBytes || audio != held.snapshot.audio.has_value() ||
        audio != candidate.containers.audio.has_value() || audio != costs.audio.has_value() ||
        audio != source.producerHolders.audio.has_value() || audio != source.runtime.audioPipeline.enabled ||
        audio != capacity.startup.audioUnits.has_value() || audio != capacity.startup.audioBytes.has_value() ||
        audio != capacity.startup.audioUnitBytes.has_value() || audio != capacity.audioAcquiringCapacity.has_value() ||
        !source.selectedCapacity->sameProduct(capacity, source.runtime.edgePolicies.metadata,
            source.runtime.edgePolicies.atomicMetadata) ||
        capacity.videoAcquiringCapacity != candidate.containers.video.acquiringObjects ||
        (audio && capacity.audioAcquiringCapacity != candidate.containers.audio->acquiringObjects) ||
        costs.video.containers.maximumPrefixObjects != candidate.containers.video.prefixObjects ||
        (audio && costs.audio->containers.maximumPrefixObjects != candidate.containers.audio->prefixObjects) ||
        !costs.totalBytes || costs.fixedBytes > costs.totalBytes ||
        costs.protocolBytes > costs.totalBytes - costs.fixedBytes ||
        costs.startupBytes != costs.totalBytes - costs.fixedBytes - costs.protocolBytes)
        return invalid("Admission materialization differs from selected capacity, membership or costs");
    return ::media::Status::success();
}

MediaPreparedRtpStreamStorageAdmission stream(const MediaPreparedRtpContainerBounds& bounds,
    const MediaPreparedStreamCandidateCosts& costs, std::size_t bytes) noexcept
{
    return {{bounds.prefixObjects, bytes}, bounds, costs.containers};
}

bool sameHolder(const MediaCompositionStreamHolderFacts& left,
    const MediaCompositionStreamHolderFacts& right) noexcept
{
    return left.decoder == right.decoder && left.maximumAtomicInputCompletion == right.maximumAtomicInputCompletion;
}

// Do not assign the complete policy: its string allocation was reserved once.
void updatePolicy(const MediaEdgePolicy& source, MediaEdgePolicy& target) noexcept
{
    target.queuePolicy = source.queuePolicy;
    target.backpressurePolicy = source.backpressurePolicy;
    auto& to = target.bufferPolicy;
    const auto& from = source.bufferPolicy;
    to.ownership = from.ownership;
    to.poolMode = from.poolMode;
    to.lifetime = from.lifetime;
    to.memoryKind = from.memoryKind;
    to.memoryBudget = from.memoryBudget;
    to.allowPoolReuse = from.allowPoolReuse;
    to.requireWritable = from.requireWritable;
    to.zeroCopyPreferred = from.zeroCopyPreferred;
    to.allowHardwareFrames = from.allowHardwareFrames;
    to.allowSoftwareTransfer = from.allowSoftwareTransfer;
}
}

::media::Result<MediaPreparedRtpStorageAdmission> MediaCompositionAdmissionMaterializer::create(
    const MediaPreparedRtpStorageSourceSnapshot& held,
    const MediaCompositionSourceCapacityCandidate& candidate,
    const MediaCompositionSourcePlanningProducts& source,
    const MediaPreparedSourceCandidateCosts& costs,
    const std::shared_ptr<MediaPreparationMemoryResource>& root)
try {
    using Result = ::media::Result<MediaPreparedRtpStorageAdmission>;
    if (auto status = validate(held, candidate, source, costs); !status)
        return Result::failure(status.error());
    if (!root || root != source.planningResource || root == held.snapshot.owner)
        return Result::failure(::media::ErrorInfo::invalidArgument("Admission backing requires the source workspace root"));
    if (auto status = MediaStandardContainerStorageReadback::validatePreparationMoves(); !status)
        return Result::failure(status.error());
    std::size_t backing = 0;
    const auto& route = candidate.capacity.route;
    const auto& holders = source.producerHolders;
    for (const auto* text : {&route.metadata.bufferPolicy.sharedAllocationGroup,
            &route.atomicMetadata.bufferPolicy.sharedAllocationGroup, &holders.video.decoder.authority,
            holders.audio ? &holders.audio->decoder.authority : nullptr}) {
        if (!text) continue;
        auto bytes = MediaStandardContainerStorageReadback::stringCopy(text->size());
        if (!bytes) return Result::failure(bytes.error());
        if (bytes.value() > (std::numeric_limits<std::size_t>::max)() - backing)
            return Result::failure(::media::ErrorInfo::allocationFailed("Admission string backing overflows"));
        backing += bytes.value();
    }
    std::optional<MediaPreparationStorageLease> lease;
    if (backing) {
        auto reserved = root->reserveStorage(backing, MediaStorageAdmission::RequiredAllocation);
        if (!reserved) return Result::failure(reserved.error());
        lease.emplace(std::move(reserved).value());
    }
    const auto bytes = held.plan->storageBytes;
    const auto& capacity = candidate.capacity;
    // Aggregate initialization directly copy-constructs each string under its
    // reservation. Moves transfer that backing through the batch to sealed.
    return Result::success(MediaPreparedRtpStorageAdmission{
        MediaPreparationBackingOwner{std::move(lease)}, bytes,
        stream(candidate.containers.video, costs.video, bytes),
        candidate.containers.audio ? std::optional{stream(*candidate.containers.audio, *costs.audio, bytes)} : std::nullopt,
        capacity.startup, capacity.acquisitionWindow, route, capacity.queues, holders,
        costs.fixedBytes, costs.protocolBytes, costs.startupBytes, costs.totalBytes, held.snapshot.owner});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedRtpStorageAdmission>::failure(
        ::media::ErrorInfo::allocationFailed("Composition admission materialization"));
}

::media::Status MediaCompositionAdmissionMaterializer::rebind(
    const MediaPreparedRtpStorageSourceSnapshot& held,
    const MediaCompositionSourceCapacityCandidate& candidate,
    const MediaCompositionSourcePlanningProducts& source,
    const MediaPreparedSourceCandidateCosts& costs,
    MediaPreparedRtpStorageAdmission& admission)
{
    const bool wasValid = admission.totalBytes != 0;
    admission.totalBytes = 0;
    if (auto status = validate(held, candidate, source, costs); !status) return status;
    const auto& route = candidate.capacity.route;
    const auto& holders = source.producerHolders;
    if (!wasValid || admission.owner != held.snapshot.owner || admission.sharedCapacityBytes != held.plan->storageBytes ||
        admission.audio.has_value() != candidate.containers.audio.has_value() ||
        admission.producerHolders.audio.has_value() != holders.audio.has_value() ||
        !sameHolder(admission.producerHolders.video, holders.video) ||
        (holders.audio && !sameHolder(*admission.producerHolders.audio, *holders.audio)) ||
        admission.producerHolders.filterRetainedInputFrames != holders.filterRetainedInputFrames ||
        admission.route.shape.members != route.shape.members || admission.route.shape.readiness != route.shape.readiness ||
        admission.route.metadata.bufferPolicy.sharedAllocationGroup != route.metadata.bufferPolicy.sharedAllocationGroup ||
        admission.route.atomicMetadata.bufferPolicy.sharedAllocationGroup != route.atomicMetadata.bufferPolicy.sharedAllocationGroup)
        return invalid("Admission refresh changed fixed backing, source owner or holder facts");
    const auto bytes = held.plan->storageBytes;
    admission.video = stream(candidate.containers.video, costs.video, bytes);
    if (admission.audio) *admission.audio = stream(*candidate.containers.audio, *costs.audio, bytes);
    admission.startup = candidate.capacity.startup;
    admission.acquisitionWindow = candidate.capacity.acquisitionWindow;
    admission.queues = candidate.capacity.queues;
    updatePolicy(route.metadata, admission.route.metadata);
    updatePolicy(route.atomicMetadata, admission.route.atomicMetadata);
    admission.fixedBytes = costs.fixedBytes;
    admission.protocolBytes = costs.protocolBytes;
    admission.startupBytes = costs.startupBytes;
    admission.totalBytes = costs.totalBytes;
    return ::media::Status::success();
}
}
