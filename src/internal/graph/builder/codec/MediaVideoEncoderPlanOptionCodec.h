#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

namespace media::ffmpeg::graph {

// Serializes an already selected encoder product without constructing a DAG.
class MediaVideoEncoderPlanOptionCodec final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaPipelineStagePlan& encoder);
private:
    MediaVideoEncoderPlanOptionCodec() = delete;
};

} // namespace media::ffmpeg::graph
