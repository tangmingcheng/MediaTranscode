#pragma once
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaAvContinuousAggregatePlanValidator final {
public:
    static ::media::Status validate(const MediaAvContinuousAggregateTopology& plan);
};
} // namespace media::ffmpeg::graph
