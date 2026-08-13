#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncPlanValidator final {
public:
    static ::media::Status validate(const MediaAvSyncPlan& plan);
    static ::media::Status validatePolicy(const MediaAvSyncPlan& plan);
    static ::media::Status validateRuntime(const MediaAvSyncPlan& plan);

private:
    MediaAvSyncPlanValidator() = delete;
};

} // namespace media::ffmpeg::graph
