#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimePlannerOptions {
    std::string edgeNodeId = "edge";
    std::string workerNodeId = "worker";
    std::string host = "127.0.0.1";
    std::string zone = "realtime";
    int basePort = 19000;

    int64_t targetLatencyUs = 50000;
    int64_t maxLatencyUs = 150000;

    bool enableGpuPlanning = true;
    bool enableMeshPlanning = true;
    bool preferZeroCopy = true;
    bool enableNodeFusion = true;
};

struct MediaRealtimePlannerResult {
    MediaGraphClusterTopology topology;
    MediaGraphPlanningPolicy policy;
    MediaGraphPlannerResult plannerResult;
};

class MediaRealtimePlanner final {
public:
    static ::media::Result<MediaRealtimePlannerResult> plan(
        const MediaGraph& graph,
        const MediaRealtimePlannerOptions& options = {});

    static MediaGraphClusterTopology buildTopology(const MediaRealtimePlannerOptions& options = {});
    static MediaGraphPlanningPolicy buildPolicy(const MediaRealtimePlannerOptions& options = {});
};

} // namespace media::ffmpeg::graph
