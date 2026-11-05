#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>
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
    MediaStartupStoreStoragePlan stores{};
    const auto add = [](std::size_t& total, std::size_t bytes) -> ::media::Status {
        auto sum = MediaCheckedArithmetic::add(total, bytes, "Startup store allocation storage");
        if (!sum) return ::media::Status::failure(sum.error());
        if (sum.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Startup store exceeds address space"));
        total = static_cast<std::size_t>(sum.value());
        return ::media::Status::success();
    };
    // Coordinator owns one unique Store per member; each Store owns a distinct
    // exact-capacity presentation snapshot beside its separately costed indexes.
    for (const auto units : {videoUnits, audioUnits.value_or(0)}) {
        if (!units) continue;
        auto object = MediaPreparationMemoryResource::allocationFootprint(
            sizeof(MediaAvStartupStreamStore), alignof(MediaAvStartupStreamStore));
        auto elements = MediaCheckedArithmetic::multiply(units, sizeof(MediaAvStartupIndexedUnit),
            "Startup presentation snapshot backing");
        if (!object) return Result::failure(object.error());
        if (!elements) return Result::failure(elements.error());
        if (elements.value() > (std::numeric_limits<std::size_t>::max)())
            return Result::failure(::media::ErrorInfo::allocationFailed("Startup snapshot exceeds address space"));
        auto snapshot = MediaPreparationMemoryResource::allocationFootprint(
            static_cast<std::size_t>(elements.value()), alignof(MediaAvStartupIndexedUnit));
        if (!snapshot) return Result::failure(snapshot.error());
        if (auto status = add(stores.objectBytes, object.value()); !status) return Result::failure(status.error());
        if (auto status = add(stores.snapshotBytes, snapshot.value()); !status) return Result::failure(status.error());
    }
    stores.preparationBytes = stores.objectBytes;
    if (auto status = add(stores.preparationBytes, stores.snapshotBytes); !status) return Result::failure(status.error());
    return Result::success({std::move(batch).value(), std::move(routeCosts).value(), std::move(clock).value(), stores});
}

} // namespace media::ffmpeg::graph
