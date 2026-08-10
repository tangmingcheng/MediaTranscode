#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodePlanningDraft;
struct MediaRealtimeRtpTranscodeRequest;
struct MediaRealtimeOutputPlanningDraft;

class MediaRealtimeVideoRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeVideoRuntimePlan> plan(
        MediaRealtimeRtpTranscodePlanningDraft& outer,
        MediaRealtimeOutputPlanningDraft output,
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaRational sourceTimeBase,
        MediaRational outputFrameRate);

private:
    MediaRealtimeVideoRuntimePlanner() = delete;
};

} // namespace media::ffmpeg::graph
