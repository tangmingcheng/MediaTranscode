#pragma once
#include "internal/graph/model/MediaRuntimeReclamationPlan.h"
#include "media_transcode/Result.h"
namespace media::ffmpeg::graph {
class MediaRealtimeReclamationPlanner final {
public:
    static ::media::Result<MediaRuntimeReclamationPlan> plan(MediaRunningTime sessionProgressTimeout);
};
} // namespace media::ffmpeg::graph
