#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"

namespace media::ffmpeg::graph {

class MediaAvSynchronizationPolicyPlanner final {
public:
    static void apply(MediaAvSyncPlan& plan);
private:
    MediaAvSynchronizationPolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph
