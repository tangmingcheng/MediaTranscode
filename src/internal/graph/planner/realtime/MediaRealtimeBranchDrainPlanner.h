#pragma once

#include "internal/graph/model/MediaRuntimeBranchDrainPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeBranchDrainPlanner final {
public:
    static ::media::Result<MediaRuntimeBranchDrainPlan> plan(
        MediaRunningTime sessionProgressTimeout);

private:
    MediaRealtimeBranchDrainPlanner() = delete;
};

} // namespace media::ffmpeg::graph
