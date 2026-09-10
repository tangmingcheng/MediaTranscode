#include "internal/graph/planner/realtime/MediaRealtimeReclamationPlanner.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
namespace media::ffmpeg::graph {
::media::Result<MediaRuntimeReclamationPlan> MediaRealtimeReclamationPlanner::plan(
    MediaRunningTime sessionProgressTimeout)
{
    using Result = ::media::Result<MediaRuntimeReclamationPlan>;
    if (sessionProgressTimeout.nanoseconds() <= 0) return Result::failure(
        ::media::ErrorInfo::invalidArgument("Reclamation requires the existing session progress timeout"));
    // One segment is reclaimed exactly once by one prepared owner. These are
    // state-machine cardinalities, not empirical thread or queue headroom.
    return Result::success({MediaRuntimeReclamationMode::DedicatedSingleShotOwner,
        1, 1, MediaRuntimeBranch::fixedStorageBytes(), sessionProgressTimeout});
}
} // namespace media::ffmpeg::graph
