#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeRtpTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaGraph> build(MediaRealtimeRtpTranscodePlan plan);
    static ::media::Result<MediaRealtimeExecutableGraph> buildExecutable(
        MediaRealtimeTranscodePreflight preflight);
    static ::media::Status validate(const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeRtpTranscodeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
