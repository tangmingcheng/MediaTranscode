#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaTsAcceptedWorkService.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <variant>
#include <span>

namespace media::ffmpeg::graph {

// Ordered, imminent obligations. Service durations are optimistic bounds,
// not reservations of the sender or shared service scope.
struct MediaTsMaintenanceServiceObligation final {
    MediaRunningTime release;
    MediaRunningTime completionDeadline;
    MediaRunningTime serviceDuration;
};

enum class MediaTsPublicationOrder {
    YieldMediaToDueMaintenance,
    PreserveSelectedMaintenance
};

// A prefix's internal service limit does not replace its AU protocol deadline.
struct MediaTsPrefixPublicationWindow final {
    MediaRunningTime release;
    MediaRunningTime accessUnitDeadline;
    MediaRunningTime completionLimit;
};

struct MediaTsMaintenancePublicationBoundary final {
    MediaRunningTime release;
};

using MediaTsPrefixPublicationDecision = std::variant<
    MediaTsPrefixPublicationWindow, MediaTsMaintenancePublicationBoundary>;

class MediaTsPrefixServicePlan;

struct MediaTsPrefixBatch final {
    std::size_t datagramCount;
    std::size_t payloadBytes;
    MediaTsPrefixPublicationWindow window;
};

struct MediaTsPrefixPublicationCapacity final {
    std::uint64_t availablePayloadBytes;
    std::size_t availableHolders;
};

using MediaTsPrefixBatchDecision = std::variant<
    MediaTsPrefixBatch, MediaTsMaintenancePublicationBoundary>;

// Evaluates the next unpublished prefix only after the prior prefix commits.
// The mux executes the returned window/boundary; it cannot extend either one.
class MediaTsPrefixPublicationPlanner final {
public:
    static ::media::Result<MediaTsPrefixBatchDecision> planBatch(
        const MediaTsUnpublishedTail& tail,
        MediaTsPrefixPublicationCapacity capacity,
        const MediaTsPrefixServicePlan& service,
        std::span<const MediaTsMaintenanceServiceObligation> maintenance,
        MediaRunningTime observedNow,
        const MediaTsPublishedWorkService& publishedWork);

    static ::media::Result<MediaTsPrefixPublicationDecision> plan(
        MediaRunningTime prefixRelease,
        MediaRunningTime accessUnitDeadline,
        std::span<const MediaTsMaintenanceServiceObligation> maintenance,
        MediaRunningTime observedNow,
        const MediaTsPublishedWorkService& publishedWork,
        MediaRunningTime requiredServiceDuration,
        MediaTsPublicationOrder order);
};

} // namespace media::ffmpeg::graph
