#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSchedulerSegmentOptions final {
    std::string prefix;
    MediaEndpoint canonicalVideo;
    MediaEndpoint canonicalAudio;
};

struct MediaRealtimeAvSchedulerSegmentResult final {
    MediaEndpoint video;
    MediaEndpoint audio;
};

class MediaRealtimeAvSchedulerSegmentBuilder final {
public:
    static ::media::Result<MediaRealtimeAvSchedulerSegmentResult> build(
        MediaGraph& graph,
        const MediaRealtimeAvSchedulerSegmentOptions& options,
        const MediaRealtimeAvSyncRuntimePlan& plan);

private:
    MediaRealtimeAvSchedulerSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
