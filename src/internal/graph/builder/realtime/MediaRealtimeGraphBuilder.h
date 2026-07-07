#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

using MediaRealtimeGraphBuilderOptions = MediaRealtimeRtpTranscodeRequest;

struct MediaRealtimeGraphBuilderResult {
    MediaGraph graph;
};

class MediaRealtimeGraphBuilder final {
public:
    static ::media::Result<MediaRealtimeGraphBuilderResult> build(
        const MediaRealtimeGraphBuilderOptions& options);

private:
    MediaRealtimeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
