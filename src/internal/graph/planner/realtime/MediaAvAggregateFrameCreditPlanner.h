#pragma once

#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaAvAggregateFrameCreditPlanner final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaAvContinuousAggregateTopology& topology);
    static ::media::Result<MediaFrameCreditContract> decode(
        const MediaNodeOptions& options, MediaStreamKind stream);
};
} // namespace media::ffmpeg::graph
