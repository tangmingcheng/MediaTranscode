#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/segments/MediaRealtimeAvOutputSegmentPlan.h"
#include "media_transcode/Result.h"

#include <string>

#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSchedulerSegmentOptions final {
    std::string prefix;
    MediaEndpoint canonicalVideo;
    MediaEndpoint canonicalAudio;
};

struct MediaRealtimeAvSchedulerSegmentResult final {
    MediaEndpoint video;
    MediaEndpoint audio;
    MediaEndpoint serialized;
    MediaNodeId scheduler;
    std::vector<MediaNodeId> outputMembers;
};

class MediaRealtimeAvSchedulerSegmentBuilder final {
public:
    static ::media::Result<MediaRealtimeAvSchedulerSegmentResult> build(
        MediaGraph& graph,
        const MediaRealtimeAvSchedulerSegmentOptions& options,
        const MediaRealtimeAvOutputSegmentPlan& plan);

private:
    MediaRealtimeAvSchedulerSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
