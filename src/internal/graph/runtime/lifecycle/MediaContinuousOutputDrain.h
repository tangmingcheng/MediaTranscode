#pragma once

#include "internal/graph/model/MediaRuntimeBranchDrainPlan.h"
#include "media_transcode/Result.h"
#include <chrono>

namespace media::ffmpeg::graph {
class MediaGraphRuntime;

// Control-thread orchestration. All media/EOF work stays on existing DAG owners.
class MediaContinuousOutputDrain final {
public:
    static ::media::Status finish(MediaGraphRuntime& runtime,
        const MediaRuntimeBranchDrainPlan& plan, std::chrono::milliseconds pollInterval);
};
} // namespace media::ffmpeg::graph
