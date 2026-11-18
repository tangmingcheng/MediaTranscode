#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanCore;
struct MediaRealtimeOutputPlanningDraft;
struct MediaAvSyncPlan;
struct MediaAudioPipelinePlan;
class MediaResolvedAudioOutputPlan;
struct MediaRealtimeRtpInputNodePlan;

struct MediaRealtimeAvSyncResolvedFacts final {
    MediaRealtimeAvSyncPlanningFacts timing;
    MediaRealtimeAvSyncAssemblyPlan assembly;
};

class MediaRealtimeAvSyncPlanningFactsResolver final {
public:
    static ::media::Result<MediaRealtimeAvOutputTimingFacts> resolveOutput(
        const MediaResolvedAudioOutputPlan& audio,
        const MediaRealtimeOutputPlanningDraft& output,
        const MediaAvSyncPlan& synchronization);
    static ::media::Result<MediaRealtimeAvSyncResolvedFacts> resolve(
        const MediaRealtimeRtpTranscodePlanCore& plan,
        const MediaAudioPipelinePlan& audio,
        const MediaRealtimeAvSyncComponentBounds& componentBounds,
        const MediaRealtimeRtpInputNodePlan* isolatedAudioInput,
        const MediaRealtimeOutputPlanningDraft& output,
        const MediaAvSyncPlan& synchronization);

private:
    MediaRealtimeAvSyncPlanningFactsResolver() = delete;
};

} // namespace media::ffmpeg::graph
