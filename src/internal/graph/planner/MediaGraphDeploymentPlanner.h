#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaGraphPlannerReport.h"
#include "internal/graph/planner/MediaGraphPlanningPolicy.h"
#include "internal/graph/runtime/distributed/MediaGraphClusterTopology.h"
#include "internal/graph/runtime/distributed/MediaGraphDeploymentPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaGraphDeploymentPlannerResult {
    MediaGraphDeploymentPlan plan;
    MediaGraphPlannerReport report;
};

class MediaGraphDeploymentPlanner final {
public:
    static ::media::Result<MediaGraphDeploymentPlannerResult> plan(
        const MediaGraph& graph,
        const MediaGraphClusterTopology& topology,
        const MediaGraphPlanningPolicy& policy = {});

private:
    static const MediaGraphClusterNode* selectNode(const MediaNode& graphNode,
                                                   const MediaGraphClusterTopology& topology,
                                                   const MediaGraphPlanningPolicy& policy,
                                                   std::size_t& roundRobinIndex);

    static const MediaGraphClusterNode* firstAvailableRole(const MediaGraphClusterTopology& topology,
                                                           MediaGraphClusterNodeRole role);

    static bool isIoNode(MediaNodeKind kind) noexcept;
};

} // namespace media::ffmpeg::graph
