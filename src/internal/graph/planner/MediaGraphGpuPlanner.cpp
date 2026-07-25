#include "internal/graph/planner/MediaGraphGpuPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphGpuPlannerResult> MediaGraphGpuPlanner::plan(
    const MediaGraph& graph,
    const MediaGraphPlanningPolicy& policy)
{
    MediaGraphGpuPlannerResult result;

    if (!policy.enableGpuPlanning) {
        result.report.info("gpu-planner", "GPU planning disabled");
        return ::media::Result<MediaGraphGpuPlannerResult>::success(std::move(result));
    }

    return ::media::Result<MediaGraphGpuPlannerResult>::failure(
        ::media::ErrorInfo::unsupported(
            "MediaGraphGpuPlanner is unsupported: CUDA/NVENC codec execution is stable, "
            "but generic GPU command execution is not implemented"));

}

} // namespace media::ffmpeg::graph
