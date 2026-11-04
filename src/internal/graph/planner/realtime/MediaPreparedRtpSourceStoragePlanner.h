#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"

namespace media::ffmpeg::graph {

// Logical owned payload and public object headers; allocator capacity, FFmpeg
// implementation-private state and device allocations are not an RSS promise.
class MediaPreparedRtpSourceStoragePlanner final {
public:
    static ::media::Result<MediaPreparedRtpContainerStoragePlan> containers(
        const MediaPreparedRtpSourceSessionStreamPlan& stream,
        MediaPreparedRtpContainerBounds bounds, bool probeSnapshot);
    static ::media::Status plan(MediaPreparedRtpSourceSessionPlan& product);
};

} // namespace media::ffmpeg::graph
