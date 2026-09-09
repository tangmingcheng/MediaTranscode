#include "internal/graph/planner/realtime/MediaRealtimeBranchDrainPlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRuntimeBranchDrainPlan> MediaRealtimeBranchDrainPlanner::plan(
    MediaRunningTime sessionProgressTimeout)
{
    if (sessionProgressTimeout.nanoseconds() <= 0) {
        return ::media::Result<MediaRuntimeBranchDrainPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Branch drain requires the explicit session media-progress timeout"));
    }
    return ::media::Result<MediaRuntimeBranchDrainPlan>::success(
        MediaRuntimeBranchDrainPlan{sessionProgressTimeout});
}

} // namespace media::ffmpeg::graph
