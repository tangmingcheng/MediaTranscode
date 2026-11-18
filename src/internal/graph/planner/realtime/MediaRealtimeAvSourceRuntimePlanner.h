#pragma once

#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceCapacityState.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {
struct MediaAvSourceCapacityFacts;
struct MediaRealtimeAvSourceClockRequest;
struct MediaRealtimeAvSourceTimingFacts;

struct MediaRealtimeAvSourceRuntimeFixedRequest {
    MediaAvSyncGroupKey groupKey;
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    const MediaRealtimeRtpInputNodePlan& input;
    int videoStreamIndex;
    std::string_view videoCodecName;
    MediaThreadingPolicy threadingPolicy;
    bool videoFilterActive;
    MediaRunningTime activationOutputLead;
    bool selectedAudio;
    MediaRtpControlDelivery rtpControlDelivery;
};

class MediaRealtimeAvSourceRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSourceFixedProducts> prepareFixed(
        const MediaRealtimeAvSourceRuntimeFixedRequest& request, MediaAudioPipelinePlan audio,
        std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput, MediaAvSyncFixedPlan synchronization);
    static ::media::Result<MediaRealtimeAvSourceCapacityState> planCapacity(
        const MediaRealtimeAvSourceFixedProducts& fixed, const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncStartupPolicyView& startup, const MediaGraphQueueParameters& queues);
    static ::media::Result<MediaRealtimeAvSourceCapacityState> planCapacity(
        MediaRealtimeAvSourceFixedProducts&&, const MediaRealtimeAvSourceClockRequest&,
        const MediaAvSyncStartupPolicyView&, const MediaGraphQueueParameters&) = delete;
    static ::media::Result<MediaRealtimeAvSourceCapacityState> planCapacity(
        const MediaRealtimeAvSourceFixedProducts&&, const MediaRealtimeAvSourceClockRequest&,
        const MediaAvSyncStartupPolicyView&, const MediaGraphQueueParameters&) = delete;
    static ::media::Result<MediaRealtimeAvSourcePlanningProducts> bindFacts(
        MediaRealtimeAvSourceFixedProducts&& fixed, MediaRealtimeAvSourceCapacityState&& capacity);
    // Prepared input/timing and all non-capacity source facts remain fixed.
    // The clock request borrows audio/isolated input from state.fixed.
    // Success allocates nothing; failure invalidates the entire candidate.
    static ::media::Status rebindCapacity(
        const MediaRealtimeAvSourceClockRequest& clockRequest,
        const MediaRealtimeAvSourceTimingFacts& timing, const MediaAvSyncStartupPolicyView& startup,
        MediaRealtimeAvSourceCapacityState& state);
    static ::media::Result<MediaRealtimeAvSourceRuntimePlan> bind(
        MediaRealtimeAvSourcePlanningProducts products,
        MediaOutputReservationStoragePlan reservationStorage);
private:
    MediaRealtimeAvSourceRuntimePlanner() = delete;
};

} // namespace media::ffmpeg::graph
