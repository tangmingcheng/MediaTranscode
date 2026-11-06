#pragma once
#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupBatchStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/model/MediaPreparedRtpStorageReservation.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include <vector>
#include "internal/graph/planner/realtime/MediaRtpClockRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpContainerStoragePlan.h"

namespace media::ffmpeg::graph {

// Source preparation owns no encoder, muxer, sender or output FIFO product.
struct MediaRealtimeCompositionSourcePlan final {
    // Workspace allocation domain, distinct from source media reservations.
    // Declared first so it outlives every catalog; assignment is deliberately disabled.
    const std::shared_ptr<MediaPreparationMemoryResource> planningResource;
    RealtimeInputType inputType;
    MediaRealtimeRtpInputNodePlan input;
    int videoStreamIndex;
    MediaVideoSourcePlan video;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    MediaRealtimeAvSourceRuntimePlan runtime;
    std::vector<MediaPreparedRtpStorageReservation> preparationStorage;
    MediaCompositionProducerHolderFacts producerHolders;
    std::optional<MediaStartupBatchStorageCosts> startupBatchStorage;
    std::optional<MediaStartupRouteStoragePlan> startupRouteStorage;
    std::optional<MediaRtpClockRouteStoragePlan> clockRouteStorage;
    std::optional<MediaStartupCoordinatorStoragePlan> startupCoordinatorStorage;
    MediaCompositionSourceBranchPlan branchConnections;
    MediaConnectionCatalog<MediaSourceInputConnection> inputConnections;
    std::optional<MediaPreparedRtpSourceContainerBounds> preparedPacketContainers;
};

} // namespace media::ffmpeg::graph
