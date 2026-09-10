#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoOutputRequest final {
    MediaRealtimeOutputConfig output;
    MediaRealtimeVideoTranscodeParameters video;
};

} // namespace media::ffmpeg::graph
