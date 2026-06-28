#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaLocalPlannerOptions {
    std::string nodeId = "local";
    std::string host = "127.0.0.1";
    std::string zone = "local";
    int port = 19000;

    bool enableGpuPlanning = true;
    bool enableMeshPlanning = true;
    bool preferZeroCopy = true;
};

struct MediaLocalPlannerResult {
    MediaGraphClusterTopology topology;
    MediaGraphPlanningPolicy policy;
    MediaGraphPlannerResult plannerResult;
};

class MediaLocalPlanner final {
public:
    static ::media::Result<MediaLocalPlannerResult> plan(
        const MediaGraph& graph,
        const MediaLocalPlannerOptions& options = {});

    static MediaGraphClusterTopology buildTopology(const MediaLocalPlannerOptions& options = {});
    static MediaGraphPlanningPolicy buildPolicy(const MediaLocalPlannerOptions& options = {});
};

} // namespace media::ffmpeg::graph
