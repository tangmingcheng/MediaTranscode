#pragma once

#include "internal/graph/builder/MediaEncodedBranchEndpoints.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

namespace media::ffmpeg::graph {

class MediaRealtimeVideoEncodingGroupBuilder final {
public:
    static ::media::Result<MediaEncodedBranchEndpoints> appendFanout(
        MediaGraph& graph,
        const std::string& prefix,
        MediaEncodedBranchEndpoints encoded,
        const MediaVideoOutputFanoutPlan& policy,
        const MediaEdgePolicy& packetPolicy);
};

} // namespace media::ffmpeg::graph
