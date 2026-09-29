#pragma once

#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceTimingFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputNodePlan;
struct MediaAudioPipelinePlan;
struct MediaAvSyncPlan;

struct MediaRealtimeAvSourceClockRequest final {
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    const MediaRealtimeRtpInputNodePlan& input;
    int videoStreamIndex;
    std::string_view videoCodecName;
    const MediaAudioPipelinePlan& audio;
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput;
};

struct MediaRealtimeAvSourceClockPlan final {
    MediaRealtimeAvSourceTimingFacts timing;
    MediaRealtimeAvSyncAssemblyPlan assembly;
};

class MediaRealtimeAvSourceClockPlanner final {
public:
    static ::media::Result<MediaRealtimeAvSourceClockPlan> plan(
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlan& synchronization);
private:
    MediaRealtimeAvSourceClockPlanner() = delete;
};

} // namespace media::ffmpeg::graph
