#pragma once

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"

namespace media::ffmpeg::graph {

class MediaRealtimeAvSyncProtocolInputBuilder final {
public:
    static ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> build(
        MediaGraph& graph,
        const MediaRealtimeAvSyncInputSegmentOptions& options,
        const MediaRealtimeAvSourceRuntimePlan& plan);

private:
    MediaRealtimeAvSyncProtocolInputBuilder() = delete;
};

} // namespace media::ffmpeg::graph
