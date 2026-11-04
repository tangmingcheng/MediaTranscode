#pragma once
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaVideoFilterExecutionPlanner final {
public:
    static ::media::Result<MediaVideoFilterExecutionPlan> forNegotiatedOutput(std::string filterDescription, MediaVideoFrameLineageProduct frameProduct);
    static ::media::Result<MediaVideoFilterExecutionPlan> sourceCopy(
        MediaRational sourceFrameRate, MediaRational sourceSampleAspectRatio,
        MediaVideoFilterIsolationEvidence evidence);
};
} // namespace media::ffmpeg::graph
