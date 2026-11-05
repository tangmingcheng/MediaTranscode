#include "internal/graph/planner/realtime/MediaStartupCoordinatorStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {
namespace {
using Bytes = ::media::Result<std::size_t>;
Bytes checked(::media::Result<std::uint64_t> value)
{
    if (!value) return Bytes::failure(value.error());
    if (value.value() > (std::numeric_limits<std::size_t>::max)())
        return Bytes::failure(::media::ErrorInfo::allocationFailed("Startup coordinator storage exceeds address space"));
    return Bytes::success(static_cast<std::size_t>(value.value()));
}
Bytes add(std::size_t left, std::size_t right)
{ return checked(MediaCheckedArithmetic::add(left, right, "Startup coordinator storage sum")); }
template<class T> Bytes request(std::size_t count)
{
    if (!count) return Bytes::success(0);
    auto bytes = checked(MediaCheckedArithmetic::multiply(count, sizeof(T), "Startup coordinator request"));
    if (!bytes) return bytes;
    return MediaPreparationMemoryResource::allocationFootprint(bytes.value(), alignof(T));
}
} // namespace

::media::Result<MediaStartupCoordinatorStoragePlan> MediaStartupCoordinatorStoragePlanner::plan(
    std::size_t videoUnits, std::optional<std::size_t> audioUnits)
{
    using Result = ::media::Result<MediaStartupCoordinatorStoragePlan>;
    if (!videoUnits || (audioUnits && !*audioUnits))
        return Result::failure(::media::ErrorInfo::invalidArgument("Startup coordinator requires positive member capacities"));
    MediaStartupCoordinatorStoragePlan result{};
    auto coordinator = request<MediaAvStartupCoordinator>(1);
    if (!coordinator) return Result::failure(coordinator.error());
    result.coordinatorBytes = coordinator.value();
    for (const auto units : {videoUnits, audioUnits.value_or(0)}) {
        if (!units) continue;
        // Unique Store and its exact-capacity snapshot are distinct requests.
        auto store = request<MediaAvStartupStreamStore>(1);
        auto snapshot = request<MediaAvStartupIndexedUnit>(units);
        auto selections = request<MediaAvStartupSelection>(units);
        if (!store) return Result::failure(store.error());
        if (!snapshot) return Result::failure(snapshot.error());
        if (!selections) return Result::failure(selections.error());
        auto stores = add(result.storeBytes, store.value());
        auto snapshots = add(result.snapshotBytes, snapshot.value());
        auto selected = add(result.selectionBytes, selections.value());
        if (!stores) return Result::failure(stores.error());
        if (!snapshots) return Result::failure(snapshots.error());
        if (!selected) return Result::failure(selected.error());
        result.storeBytes = stores.value();
        result.snapshotBytes = snapshots.value();
        result.selectionBytes = selected.value();
    }
    auto buffered = add(videoUnits, audioUnits.value_or(0));
    if (!buffered) return Result::failure(buffered.error());
    result.maximumPriorIds = buffered.value();
    // advanceGeneration can retain IDs from the old stores. purge constructs a
    // separate result from prior IDs, current store IDs and one rejected input.
    // tryRelease uses prefixes instead of whole stores and has no trailing ID.
    // Keep a conservative sum across these branches, not an empirical multiplier.
    auto resultIds = add(result.maximumPriorIds, buffered.value());
    if (!resultIds) return Result::failure(resultIds.error());
    resultIds = add(resultIds.value(), 1);
    if (!resultIds) return Result::failure(resultIds.error());
    result.maximumResultIds = resultIds.value();
    auto prior = request<MediaAvStartupUnitId>(result.maximumPriorIds);
    auto ids = request<MediaAvStartupUnitId>(result.maximumResultIds);
    if (!prior) return Result::failure(prior.error());
    if (!ids) return Result::failure(ids.error());
    result.priorIdsBytes = prior.value();
    result.resultIdsBytes = ids.value();
    // Node consumes one decision synchronously. Selection and ID arrays can
    // coexist with the released arrays, which the route product counts separately.
    for (const auto bytes : {result.coordinatorBytes, result.storeBytes, result.snapshotBytes,
             result.selectionBytes, result.priorIdsBytes, result.resultIdsBytes}) {
        auto total = add(result.preparationBytes, bytes);
        if (!total) return Result::failure(total.error());
        result.preparationBytes = total.value();
    }
    return Result::success(result);
}
} // namespace media::ffmpeg::graph
