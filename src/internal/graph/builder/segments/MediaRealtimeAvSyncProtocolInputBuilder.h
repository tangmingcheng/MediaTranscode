#pragma once

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncProtocolInputEndpoints final {
    MediaEndpoint video;
    MediaEndpoint audio;
    MediaEndpoint sourceClock;
};

class MediaRealtimeAvSyncProtocolInputBuilder final {
public:
    static ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> build(
        MediaGraph& graph,
        const MediaRealtimeAvSyncInputSegmentOptions& options,
        const MediaRealtimeAvSyncRuntimePlan& plan);

private:
    MediaRealtimeAvSyncProtocolInputBuilder() = delete;
};

} // namespace media::ffmpeg::graph
