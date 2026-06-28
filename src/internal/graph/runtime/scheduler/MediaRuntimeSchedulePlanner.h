#pragma once

#include "internal/graph/runtime/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/scheduler/MediaRuntimeSchedulePlan.h"
#include "internal/graph/runtime/scheduler/MediaRuntimeSchedulerPolicy.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRuntimeSchedulePlanner final {
public:
    static ::media::Result<MediaRuntimeSchedulePlan> plan(
        const MediaGraphExecutionContext& context,
        const MediaRuntimeSchedulerPolicy& policy = {});
};

} // namespace media::ffmpeg::graph
