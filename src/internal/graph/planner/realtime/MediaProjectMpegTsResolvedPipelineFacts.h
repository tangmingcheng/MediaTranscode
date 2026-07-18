#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaProjectMpegTsResolvedPipelineFacts final {
    std::string videoCodecName;
    MediaEncodedPacketLayout videoPacketLayout;
    MediaResolvedAudioOutputPlan audioOutput;
};

} // namespace media::ffmpeg::graph
