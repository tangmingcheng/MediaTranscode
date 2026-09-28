#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSourceClockRequest;
struct MediaAvSyncPlan;
struct MediaRealtimeAvSourceTimingFacts;
struct MediaRealtimeAvSyncAssemblyPlan;

class MediaRealtimeAvSyncRuntimeInputValidator final {
public:
    static ::media::Status validate(
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlan& synchronization,
        const MediaRealtimeAvSourceTimingFacts& facts,
        const MediaRealtimeAvSyncAssemblyPlan& assembly);

private:
    MediaRealtimeAvSyncRuntimeInputValidator() = delete;
};

} // namespace media::ffmpeg::graph
