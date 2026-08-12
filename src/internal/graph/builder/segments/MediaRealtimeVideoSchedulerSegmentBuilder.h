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

struct MediaRealtimeVideoSchedulerSegmentResult final {
    MediaEndpoint activation;
    MediaEndpoint scheduledVideo;
};

class MediaRealtimeVideoSchedulerSegmentBuilder final {
public:
    static ::media::Result<MediaRealtimeVideoSchedulerSegmentResult> build(
        MediaGraph& graph,
        const MediaRealtimeVideoSchedulerSegmentOptions& options,
        const MediaRealtimeVideoRuntimePlan& plan);

private:
    MediaRealtimeVideoSchedulerSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
