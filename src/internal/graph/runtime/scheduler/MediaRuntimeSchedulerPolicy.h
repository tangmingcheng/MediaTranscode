#pragma once

#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/scheduler/MediaRuntimeSchedulePlan.h"

namespace media::ffmpeg::graph {

struct MediaRuntimeSchedulerPolicy {
    MediaRuntimeScheduleStrategy strategy = MediaRuntimeScheduleStrategy::GraphOrder;
    MediaThreadingPolicy threadingPolicy;
    bool collectMetrics = true;
    bool allowParallelNodes = false;
};

} // namespace media::ffmpeg::graph
