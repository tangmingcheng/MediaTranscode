#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

namespace media_transcode::test {

struct ScheduledRtpOutputIntegrationGraph final {
    ::media::ffmpeg::graph::MediaGraph graph;
    ::media::ffmpeg::graph::MediaNodeId binder;
    ::media::ffmpeg::graph::MediaNodeId scheduler;
    ::media::ffmpeg::graph::MediaNodeId router;
    ::media::ffmpeg::graph::MediaNodeId videoSender;
    ::media::ffmpeg::graph::MediaNodeId audioSender;
    ::media::ffmpeg::graph::MediaNodeId publisher;
};

class ScheduledRtpOutputIntegrationGraphBuilder final {
public:
    static ::media::Result<ScheduledRtpOutputIntegrationGraph> build(
        const ::media::ffmpeg::graph::MediaRealtimeAvSyncRuntimePlan& plan);

private:
    ScheduledRtpOutputIntegrationGraphBuilder() = delete;
};

} // namespace media_transcode::test
