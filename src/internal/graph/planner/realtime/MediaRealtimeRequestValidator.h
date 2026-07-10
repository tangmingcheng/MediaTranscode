#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

#include <media_transcode/Result.h>

namespace media::ffmpeg::graph {

class MediaRealtimeRequestValidator final {
public:
    static ::media::Status validate(const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeRequestValidator() = delete;
};

} // namespace media::ffmpeg::graph
