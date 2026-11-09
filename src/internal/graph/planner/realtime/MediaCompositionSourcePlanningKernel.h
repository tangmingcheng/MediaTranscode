#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceFixedProducts.h"
#include "internal/graph/planner/realtime/MediaRtpClockRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupBatchStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedInputRetentionPlan.h"
#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
namespace media::ffmpeg::graph {
struct MediaAvSourceCapacityFacts;
struct MediaRealtimeAvSourceTimingFacts;
struct MediaCompositionSourceFixedRequest {
    const MediaRealtimeRtpTranscodeRequest& sourceRequest;
    const MediaRealtimeRawInputPlan& raw;
    const MediaVideoSourceTiming& timing;
    const MediaRtpIngressPlan& videoIngress;
    const MediaRtpIngressPlan* audioIngress;
    const MediaPreparedInputRetentionStream* audioRetention;
    const MediaVideoSourcePlan& video;
    const MediaResolvedAudioOutputPlan& outputAudio;
    const std::optional<MediaSelectedAudioResampler>& preparedAudioResampler;
    const MediaRealtimeDeploymentEnvelope& deployment;
    MediaRunningTime activationOutputLead;
    bool selectedAudio;
};
// Original request/raw/deployment/retention/video owners must remain stable and
// outlive preparation, every candidate, and binding. Runtime declarations are owned.
struct MediaCompositionSourceFixedProducts final {
    // Shared across moves; declared before every covered owning field.
    std::shared_ptr<const MediaPreparationStorageLease> fixedStorage;
    const std::shared_ptr<MediaPreparationMemoryResource> planningResource;
    const MediaRealtimeRtpTranscodeRequest& sourceRequest;
    const MediaRealtimeRawInputPlan& raw;
    const MediaRealtimeDeploymentEnvelope& deployment;
    const MediaPreparedInputRetentionStream* audioRetention;
    MediaVideoSourceTiming timing;
    RealtimeInputType inputType;
    MediaRealtimeRtpInputNodePlan input;
    int videoStreamIndex;
    const MediaVideoSourcePlan& video;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    MediaRealtimeAvSourceFixedProducts runtime;
    MediaCompositionProducerHolderFacts producerHolders;
};
// Video remains borrowed from the original stable owner through all candidate
// use. Other facts and workspace resource are owned; no source media reservation.
struct MediaCompositionSourcePlanningProducts final {
    // Shared across moves; declared before every covered owning field.
    std::shared_ptr<const MediaPreparationStorageLease> fixedStorage;
    // Workspace allocation domain, distinct from source media reservations.
    // Declared first so it outlives every catalog; assignment is deliberately disabled.
    const std::shared_ptr<MediaPreparationMemoryResource> planningResource;
    RealtimeInputType inputType;
    MediaRealtimeRtpInputNodePlan input;
    int videoStreamIndex;
    const MediaVideoSourcePlan& video;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    MediaRealtimeAvSourcePlanningProducts runtime;
    MediaCompositionProducerHolderFacts producerHolders;
    std::optional<MediaStartupBatchStorageCosts> startupBatchStorage;
    std::optional<MediaStartupRouteStoragePlan> startupRouteStorage;
    std::optional<MediaRtpClockRouteStoragePlan> clockRouteStorage;
    std::optional<MediaStartupCoordinatorStoragePlan> startupCoordinatorStorage;
    MediaCompositionSourceBranchPlan branchConnections;
    MediaPlanningCatalog<MediaSourceInputConnection> inputConnections;
};
class MediaCompositionSourcePlanningKernel final {
public:
    static ::media::Result<MediaCompositionSourceFixedProducts> prepareFixed(
        MediaCompositionSourceFixedRequest request, std::shared_ptr<MediaPreparationMemoryResource> resource);
    // Consumes the same fixed declarations after planning actual capacity.
    static ::media::Result<MediaCompositionSourcePlanningProducts> bind(
        MediaCompositionSourceFixedProducts&& fixed, MediaAvSyncCapacityFacts capacity);
    // Uses the original immutable prepared request/timing; only capacity facts
    // change. Success allocates nothing; failure invalidates all products.
    static ::media::Status rebindCapacity(
        const MediaCompositionSourceFixedRequest& prepared,
        const MediaAvSourceCapacityFacts& capacity,
        MediaCompositionSourcePlanningProducts& products);
};
}
