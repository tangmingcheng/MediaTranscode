#pragma once

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputEndpoints.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaDemuxClockInputSegmentOptions final {
    std::string prefix;
    MediaEndpoint videoPacket;
    MediaEndpoint audioPacket;
    MediaAvSyncGroupKey syncGroup;
    MediaEdgePolicy packetPolicy;
};

struct MediaDemuxClockInputEndpoints final {
    MediaEndpoint video;
    MediaEndpoint audio;
    MediaEndpoint sourceClock;
};

class MediaDemuxClockInputSegmentBuilder final {
public:
    static ::media::Result<MediaDemuxClockInputEndpoints> build(
        MediaGraph& graph,
        const MediaDemuxClockInputSegmentOptions& options,
        const MediaDemuxTimestampInputClockAssemblyPlan& plan);

private:
    MediaDemuxClockInputSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
