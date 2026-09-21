#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputEndpoints.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeInputGraph final {
    MediaEndpoint videoFormat;
    MediaEndpoint audioFormat;
    MediaEndpoint videoPacket;
    MediaEndpoint audioPacket;
    std::optional<MediaRealtimeAvSyncInputEndpoints> synchronized;
    std::vector<MediaNodeId> sourceMembers;
};

class MediaRealtimeInputGraphBuilder final {
public:
    static ::media::Result<MediaRealtimeInputGraph> append(
        MediaGraph& graph, const std::string& prefix,
        const MediaRealtimeRtpTranscodePlan& plan);
};

} // namespace media::ffmpeg::graph
