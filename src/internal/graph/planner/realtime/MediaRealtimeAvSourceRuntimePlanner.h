#pragma once

#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {
struct MediaAvSourceCapacityFacts;

struct MediaRealtimeAvSourceRuntimeRequest final {
    MediaAvSyncGroupKey groupKey;
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    const MediaRealtimeRtpInputNodePlan& input;
    int videoStreamIndex;
    std::string_view videoCodecName;
    MediaGraphQueueParameters queues;
    MediaThreadingPolicy threadingPolicy;
    bool videoFilterActive;
    MediaRunningTime activationOutputLead;
    bool selectedAudio;
    const MediaAvSourceCapacityFacts* sourceCapacity;
};

class MediaRealtimeAvSourceRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvSourcePlanningProducts> planFacts(
        const MediaRealtimeAvSourceRuntimeRequest& request,
        MediaAudioPipelinePlan audio,
        std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput,
        MediaAvSyncPlan synchronization);
    static ::media::Result<MediaRealtimeAvSourceRuntimePlan> bind(
        MediaRealtimeAvSourcePlanningProducts products,
        MediaOutputReservationStoragePlan reservationStorage);
private:
    MediaRealtimeAvSourceRuntimePlanner() = delete;
};

} // namespace media::ffmpeg::graph
