#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaScheduledMpegTsOutputSegmentOptions final {
    std::string prefix;
    MediaEndpoint epochActivated;
    MediaEndpoint videoCodec;
    MediaEndpoint audioCodec;
    MediaEndpoint scheduled;
    bool expectVideo;
    bool expectAudio;
};

struct MediaScheduledMpegTsOutputSegmentResult final {
    MediaNodeId planSource;
    MediaNodeId adapter;
    MediaNodeId udpOutput;
    MediaNodeId mux;
    MediaNodeId rtpSdpPublisher;
};

struct MediaVideoOnlyScheduledMpegTsOutputSegmentOptions final {
    std::string prefix;
    MediaEndpoint activation;
    MediaEndpoint videoCodec;
    MediaEndpoint scheduledVideo;
};

class MediaScheduledMpegTsOutputSegmentBuilder final {
public:
    static ::media::Result<MediaScheduledMpegTsOutputSegmentResult> build(
        MediaGraph& graph,
        const MediaScheduledMpegTsOutputSegmentOptions& options,
        const MediaRealtimeAvSyncRuntimePlan& plan);
    static ::media::Result<MediaScheduledMpegTsOutputSegmentResult>
    buildVideoOnly(
        MediaGraph& graph,
        const MediaVideoOnlyScheduledMpegTsOutputSegmentOptions& options,
        const MediaRealtimeVideoRuntimePlan& plan);

private:
    MediaScheduledMpegTsOutputSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
