#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoSchedulerSegmentOptions final {
    std::string prefix;
    MediaEndpoint encodedVideo;
};

class MediaRealtimeVideoSchedulerSegmentBuilder final {
public:
    static ::media::Result<MediaEndpoint> build(
        MediaGraph& graph,
        const MediaRealtimeVideoSchedulerSegmentOptions& options,
        const MediaRealtimeVideoRuntimePlan& plan);

private:
    MediaRealtimeVideoSchedulerSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
