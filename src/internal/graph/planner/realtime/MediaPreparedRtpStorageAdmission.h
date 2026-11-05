#pragma once
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
struct MediaPreparedRtpStorageSnapshot final {
    MediaPreparedRtpStreamStorageSnapshot video;
    std::optional<MediaPreparedRtpStreamStorageSnapshot> audio;
    std::uint64_t revision;
    MediaPreparationStorageSnapshot budget;
    std::shared_ptr<MediaPreparationMemoryResource> owner;
};
struct MediaPreparedRtpStreamStorageAdmission final {
    MediaPreparedRtpAdmissionStream compressed;
    MediaPreparedRtpContainerBounds bounds;
    MediaPreparedRtpContainerStoragePlan storage;
};
struct MediaPreparedRtpStorageAdmission final {
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
};
struct MediaPreparedRtpStorageSourceSnapshot final {
    const MediaPreparedRtpSourceSessionPlan* plan;
    MediaPreparedRtpStorageSnapshot snapshot;
};
struct MediaPreparedRtpBatchStoragePlan final {
    std::size_t sourceCount;
    std::shared_ptr<MediaPreparationMemoryResource> owner;
    std::size_t scratchBytes;
    std::size_t resultBytes;
    std::size_t totalBytes;
};
class MediaPreparedRtpBatchAdmissionSolver {
public:
    virtual ~MediaPreparedRtpBatchAdmissionSolver() = default;
    virtual ::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>> solve(
        std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
        const MediaStoragePartitionSnapshot<MediaPreparationStorageDomain>& partition,
        // Source totals exclude this batch product. Return exactly sourceCount
        // elements/capacity on owner; these arrays are charged to source zero.
        const MediaPreparedRtpBatchStoragePlan& batchStorage) const = 0;
};
} // namespace media::ffmpeg::graph
