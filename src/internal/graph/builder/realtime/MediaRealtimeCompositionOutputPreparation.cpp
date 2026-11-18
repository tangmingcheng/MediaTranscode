#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaVideoOutputPreparationPlanner.h"
namespace media::ffmpeg::graph {
::media::Result<MediaVideoOutputPreparationPlan> MediaRealtimeCompositionTopology::planOutputPreparation(
    const MediaVideoCanvasAllocation& allocation) const
{
    using Result = ::media::Result<MediaVideoOutputPreparationPlan>;
    auto retention = planCanvasRetention();
    if (!retention) return Result::failure(retention.error());
    return MediaVideoOutputPreparationPlanner::plan(retention.value(), allocation);
}
} // namespace media::ffmpeg::graph
