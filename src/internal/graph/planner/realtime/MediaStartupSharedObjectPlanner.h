#pragma once
#include "internal/graph/model/MediaStartupSharedObjectPlan.h"
namespace media::ffmpeg::graph {
class MediaStartupSharedObjectPlanner final {
public:
    static ::media::Result<MediaPreparedStartupSharedObject> prepared(MediaStartupSharedObjectRole role);
    static ::media::Result<MediaStartupSharedObjectPlan> plan(
        MediaStartupSharedObjectRole role, MediaLineageAllocationScope allocation);
    static ::media::Status validate(const MediaStartupSharedObjectPlan& plan,
        MediaStartupSharedObjectRole role);
};
} // namespace media::ffmpeg::graph
