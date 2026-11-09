#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"

namespace media::ffmpeg::graph {

// Logical owned payload and public object headers; allocator capacity, FFmpeg
// implementation-private state and device allocations are not an RSS promise.
class MediaPreparedRtpSourceStoragePlanner final {
public:
    // Conservative output increment of one serialized consume/tick call.
    // Does not cover prior occupancy or guarantee continuous consumer progress.
    static ::media::Result<std::size_t> completionOutputObjects(
        const MediaPreparedRtpSourceSessionStreamPlan& stream);
    // Forms minimum future capacities from the lock-held snapshot.
    // Retained history backing is separately preserved by storage accounting.
    static ::media::Result<MediaPreparedRtpContainerBounds> minimumBounds(
        const MediaPreparedRtpSourceSessionStreamPlan& stream,
        const MediaPreparedRtpStreamStorageSnapshot& held);
    static ::media::Result<MediaPreparedRtpContainerStoragePlan> containers(
        const MediaPreparedRtpSourceSessionStreamPlan& stream,
        MediaPreparedRtpContainerBounds bounds, bool probeSnapshot);
    static ::media::Status plan(MediaPreparedRtpSourceSessionPlan& product);
};

} // namespace media::ffmpeg::graph
