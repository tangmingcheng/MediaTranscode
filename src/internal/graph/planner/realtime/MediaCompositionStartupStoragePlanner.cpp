#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaCompositionStartupStoragePlan> MediaCompositionStartupStoragePlanner::plan(
    const MediaStartupRoutePlan& route, std::size_t videoUnits,
    std::optional<std::size_t> audioUnits, std::size_t groupKeyCharacters,
    std::span<const std::size_t> maximumDatagramBytes)
{
    using Result = ::media::Result<MediaCompositionStartupStoragePlan>;
    auto layout = MediaOutputReservationStoragePlanner::layout();
    if (!layout) return Result::failure(layout.error());
    auto batch = MediaStartupBatchStoragePlanner::plan(
        route, videoUnits, audioUnits, layout.value(), groupKeyCharacters);
    if (!batch) return Result::failure(batch.error());
    auto routeCosts = MediaStartupRouteStoragePlanner::plan(route, videoUnits, audioUnits, batch.value());
    if (!routeCosts) return Result::failure(routeCosts.error());
    auto clock = MediaRtpClockRouteStoragePlanner::plan(
        route.shape.members, route.metadata, maximumDatagramBytes);
    if (!clock) return Result::failure(clock.error());
    return Result::success({std::move(batch).value(), std::move(routeCosts).value(), std::move(clock).value()});
}

} // namespace media::ffmpeg::graph
