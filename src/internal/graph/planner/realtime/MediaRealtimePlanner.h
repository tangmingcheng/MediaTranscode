#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlanner.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimePlannerOptions {
    std::string edgeNodeId;
    std::string workerNodeId;
    std::string host;
    std::string zone;
    int basePort;

    int64_t targetLatencyUs;
    int64_t maxLatencyUs;

    bool enableGpuPlanning;
    bool enableMeshPlanning;
    bool preferZeroCopy;
    bool enableNodeFusion;
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
        const MediaRealtimePlannerOptions& options);

    static MediaGraphClusterTopology buildTopology(const MediaRealtimePlannerOptions& options);
    static MediaGraphPlanningPolicy buildPolicy(const MediaRealtimePlannerOptions& options);
};

} // namespace media::ffmpeg::graph
