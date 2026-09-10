#pragma once
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaVideoFilterExecutionPlanCodec final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaVideoFilterExecutionPlan& plan);
    static ::media::Result<MediaVideoFilterExecutionPlan> decode(const MediaNodeOptions& options);
};
} // namespace media::ffmpeg::graph
