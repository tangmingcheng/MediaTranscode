#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
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
struct MediaCompositionSourcePlanningRequest final {
    const MediaRealtimeRtpTranscodeRequest& sourceRequest;
    const MediaRealtimeRawInputPlan& raw;
    const MediaVideoSourceTiming& timing;
    const MediaRtpIngressPlan& videoIngress;
    const MediaRtpIngressPlan* audioIngress;
    const MediaPreparedInputRetentionStream* audioRetention;
    MediaAvSyncCapacityFacts capacity;
    MediaVideoSourcePlan video;
    const MediaResolvedAudioOutputPlan& outputAudio;
    const MediaRealtimeDeploymentEnvelope& deployment;
    MediaRunningTime activationOutputLead;
    bool selectedAudio;
};
// Request references are borrowed for this synchronous call only. Returned
// products own their facts and contain no source storage owner or reservation.
struct MediaCompositionSourcePlanningProducts final {
    RealtimeInputType inputType;
    MediaRealtimeRtpInputNodePlan input;
    int videoStreamIndex;
    MediaVideoSourcePlan video;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    MediaRealtimeAvSourcePlanningProducts runtime;
    MediaCompositionProducerHolderFacts producerHolders;
    std::optional<MediaStartupBatchStorageCosts> startupBatchStorage;
    std::optional<MediaStartupRouteStoragePlan> startupRouteStorage;
    std::optional<MediaRtpClockRouteStoragePlan> clockRouteStorage;
    std::optional<MediaStartupCoordinatorStoragePlan> startupCoordinatorStorage;
    MediaCompositionSourceBranchPlan branchConnections;
    std::vector<MediaSourceInputConnection> inputConnections;
};
class MediaCompositionSourcePlanningKernel final {
public:
    // Uses the original immutable prepared request/timing; only capacity facts
    // change. Success allocates nothing; failure invalidates all products.
    static ::media::Status rebindCapacity(
        const MediaCompositionSourcePlanningRequest& prepared,
        const MediaAvSourceCapacityFacts& capacity, const MediaRealtimeAvSourceTimingFacts& clockTiming,
        MediaCompositionSourcePlanningProducts& products);
    static ::media::Result<MediaCompositionSourcePlanningProducts> plan(MediaCompositionSourcePlanningRequest request);
};
}
