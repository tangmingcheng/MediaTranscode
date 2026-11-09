#pragma once
#include "internal/graph/runtime/resource/MediaPreparationBackingOwner.h"
#include "internal/graph/runtime/resource/MediaPreparedRtpSourceBudgets.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpContainerStoragePlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAdmissionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/model/MediaStartupRoutePlan.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {
struct MediaPreparedRtpSourceSessionPlan;

struct MediaPreparedRtpStreamStorageSnapshot final {
    std::size_t admittedUnits;
    std::size_t prefixObjects;
    std::size_t acquiringObjects;
    std::size_t packetClockHeldObjects;
    std::size_t pendingLeases;
    std::size_t pendingCapacity;
    std::size_t outputObjects;
    // Observed successful-push high water, never the configured ceiling.
    MediaPreparedRtpContainerBounds history;
};
struct MediaPreparedRtpBudgetRoles final {
    std::size_t metadata;
    std::size_t videoPayload;
    std::optional<std::size_t> audioPayload;
};
struct MediaPreparedRtpSourceBudgetSnapshot final {
    MediaPreparationStorageSnapshot metadata;
    MediaPreparationStorageSnapshot videoPayload;
    std::optional<MediaPreparationStorageSnapshot> audioPayload;
    // Disjoint direct children of one root, observed together while source is locked.
    std::size_t actualBytes() const noexcept
    { return metadata.actualRetainedBytes + videoPayload.actualRetainedBytes +
        (audioPayload ? audioPayload->actualRetainedBytes : 0); }
    std::size_t chargedBytes() const noexcept
    { return metadata.chargedBytes + videoPayload.chargedBytes +
        (audioPayload ? audioPayload->chargedBytes : 0); }
    bool sealed() const noexcept
    { return metadata.sealed || videoPayload.sealed || (audioPayload && audioPayload->sealed); }
};
struct MediaPreparedRtpStorageSnapshot final {
    MediaPreparedRtpStreamStorageSnapshot video;
    std::optional<MediaPreparedRtpStreamStorageSnapshot> audio;
    std::uint64_t revision;
    MediaPreparedRtpSourceBudgetSnapshot budgets;
    MediaPreparedRtpSourceBudgets owners;
    MediaPreparedRtpBudgetRoles roles;
    std::shared_ptr<MediaPreparationMemoryResource> owner;
};
struct MediaPreparedRtpStreamStorageAdmission final {
    MediaPreparedRtpAdmissionStream compressed;
    MediaPreparedRtpContainerBounds bounds;
    MediaPreparedRtpContainerStoragePlan storage;
};
struct MediaPreparedRtpStorageAdmission final {
    // Root-charged ordinary backing survives transfer into the sealed source.
    MediaPreparationBackingOwner preparationOwner;
    std::size_t sharedCapacityBytes;
    MediaPreparedRtpStreamStorageAdmission video;
    std::optional<MediaPreparedRtpStreamStorageAdmission> audio;
    MediaRealtimeMediaCapacityPlan startup;
    MediaRunningTime acquisitionWindow;
    MediaStartupRoutePlan route;
    MediaGraphQueueParameters queues;
    MediaCompositionProducerHolderFacts producerHolders;
    std::size_t fixedBytes;
    std::size_t protocolBytes;
    std::size_t startupBytes;
    std::size_t totalBytes;
    std::shared_ptr<MediaPreparationMemoryResource> owner;
    MediaPreparedRtpSourceBudgets budgets;
};
struct MediaPreparedRtpStorageSourceSnapshot final {
    const MediaPreparedRtpSourceSessionPlan* plan;
    MediaPreparedRtpStorageSnapshot snapshot;
};
struct MediaPreparedRtpBatchStoragePlan final {
    std::size_t sourceCount;
    std::size_t childCount;
    std::shared_ptr<MediaPreparationMemoryResource> owner;
    std::size_t scratchBytes;
    std::size_t resultBytes;
    std::size_t totalBytes;
};
// Borrowed only for the lock-held solve call. The owning freeze transaction
// keeps both exact-size arrays alive; refresh uses the existing ledger lock.
// A refreshed observation is not a reservation of future root headroom.
struct MediaPreparedRtpBatchBudgetView final {
    std::span<const std::shared_ptr<MediaPreparationStorageBudget>> children;
    std::span<MediaPreparationStorageSnapshot> snapshots;

    MediaPreparedRtpSourceBudgetSnapshot source(const MediaPreparedRtpBudgetRoles& roles) const
    {
        return {snapshots[roles.metadata], snapshots[roles.videoPayload],
            roles.audioPayload ? std::optional{snapshots[*roles.audioPayload]} : std::nullopt};
    }
    bool matches(const MediaPreparedRtpStorageSnapshot& source) const noexcept
    {
        const auto& roles = source.roles;
        if (roles.metadata >= children.size() || roles.videoPayload >= children.size() ||
            roles.metadata == roles.videoPayload ||
            roles.audioPayload.has_value() != static_cast<bool>(source.owners.audioPayload) ||
            roles.audioPayload.has_value() != source.audio.has_value()) return false;
        if (roles.audioPayload && (*roles.audioPayload >= children.size() ||
            *roles.audioPayload == roles.metadata || *roles.audioPayload == roles.videoPayload)) return false;
        return children[roles.metadata] == source.owners.metadata &&
            children[roles.videoPayload] == source.owners.videoPayload &&
            (!roles.audioPayload || children[*roles.audioPayload] == source.owners.audioPayload);
    }
    ::media::Result<MediaStoragePartitionSnapshot<MediaPreparationStorageDomain>> refresh() const
    {
        return MediaPreparationStorageBudget::snapshotPartition(children, snapshots);
    }
};
class MediaPreparedRtpBatchAdmissionSolver {
public:
    virtual ~MediaPreparedRtpBatchAdmissionSolver() = default;
    virtual ::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>> solve(
        std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
        const MediaPreparedRtpBatchBudgetView& budgets,
        // Source totals exclude this batch product. Return exactly sourceCount
        // elements/capacity on owner; these arrays are charged to source zero.
        const MediaPreparedRtpBatchStoragePlan& batchStorage) const = 0;
};
} // namespace media::ffmpeg::graph
