#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaScheduledRtpOutputSegmentOptions final {
    std::string prefix;
    MediaEndpoint epochActivated;
    MediaEndpoint videoCodec;
    MediaEndpoint audioCodec;
    MediaEndpoint scheduledVideo;
    MediaEndpoint scheduledAudio;
};

struct MediaScheduledRtpOutputSegmentResult final {
    MediaNodeId videoSender;
    MediaNodeId audioSender;
    MediaNodeId sdpPublisher;
};

class MediaScheduledRtpOutputSegmentBuilder final {
public:
    static ::media::Result<MediaScheduledRtpOutputSegmentResult> build(
        MediaGraph& graph,
        const MediaScheduledRtpOutputSegmentOptions& options,
        const MediaRealtimeAvSyncRuntimePlan& plan);

private:
    MediaScheduledRtpOutputSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
