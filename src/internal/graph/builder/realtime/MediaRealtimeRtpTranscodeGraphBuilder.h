#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeRtpTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Status validate(const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeRtpTranscodeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
