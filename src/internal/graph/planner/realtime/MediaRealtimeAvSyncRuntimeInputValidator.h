#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanView.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyView.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSourceClockRequest;
struct MediaRealtimeAvSourceTimingFacts;

class MediaRealtimeAvSyncRuntimeInputValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlanView& synchronization,
        const MediaRealtimeAvSourceTimingFacts& facts,
        const MediaRealtimeAvSyncAssemblyView& assembly);

private:
    MediaRealtimeAvSyncRuntimeInputValidator() = delete;
};

} // namespace media::ffmpeg::graph
