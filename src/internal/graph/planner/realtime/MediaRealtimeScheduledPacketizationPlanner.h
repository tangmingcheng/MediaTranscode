#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
namespace media::ffmpeg::graph {
class MediaRealtimeScheduledPacketizationPlanner final {
public:
    static ::media::Result<MediaScheduledRtpPacketizationPlan> planVideo(
        const MediaVideoOutputPlan& plan,
        const MediaRealtimeScheduledRtpOutputPlanningDraft& output,
        int clockRate, int payloadType);
    static ::media::Status planAudioVideo(const MediaVideoOutputPlan& plan,
        const MediaAudioPipelinePlan& audio, MediaRealtimeOutputPlanningDraft& output,
        const MediaAvSyncPlan& synchronization);
};
} // namespace media::ffmpeg::graph
