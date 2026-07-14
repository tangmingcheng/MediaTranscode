#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaProjectMpegTsResolvedPipelineFacts final {
    std::string videoCodecName;
    MediaResolvedAudioOutputPlan audioOutput;
};

} // namespace media::ffmpeg::graph
