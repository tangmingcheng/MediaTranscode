#pragma once

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputEndpoints.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncInputSources final {
    MediaEndpoint videoPacket;
    MediaEndpoint audioPacket;
    MediaEndpoint protocolClock;
};

struct MediaRealtimeAvSyncInputSegmentOptions final {
    std::string prefix;
    MediaRealtimeAvSyncInputSources sources;
    int releasedVideoStreamIndex = invalidMediaStreamIndex;
    int releasedAudioStreamIndex = invalidMediaStreamIndex;
    MediaEdgeKind releasedVideoEdgeKind = MediaEdgeKind::Unknown;
    MediaEdgeKind releasedAudioEdgeKind = MediaEdgeKind::Unknown;
};

class MediaRealtimeAvSyncInputSegmentBuilder final {
public:
    static ::media::Result<MediaRealtimeAvSyncInputEndpoints> build(
        MediaGraph& graph,
        const MediaRealtimeAvSyncInputSegmentOptions& options,
        const MediaRealtimeAvSyncRuntimePlan& plan);

private:
    MediaRealtimeAvSyncInputSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
