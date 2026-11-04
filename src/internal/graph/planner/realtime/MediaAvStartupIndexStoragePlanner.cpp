#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaPreparedStartupIndexStorage> prepare(
    std::size_t capacity, MediaVideoStartupReadiness readiness,
    const MediaAvStartupIndexGeometries& geometry)
{
    using R = ::media::Result<MediaPreparedStartupIndexStorage>;
    const std::uint64_t count = capacity;
    const std::uint64_t coverage = readiness == MediaVideoStartupReadiness::IntervalCoverage ? count : 0;
    auto ordinary = MediaCheckedArithmetic::add(count, 1, "Startup index node and head count");
    auto coverageHeads = MediaCheckedArithmetic::add(coverage, 1, "Startup coverage node and head count");
    if (!ordinary || !coverageHeads) return R::failure(!ordinary ? ordinary.error() : coverageHeads.error());
    std::uint64_t pending = 0;
    if (coverage) {
        // Each later interval can occur once in each earlier candidate. Divide
        // an even factor first; no N*N intermediate or sampled growth estimate.
        const auto left = coverage % 2 == 0 ? coverage / 2 : coverage;
        const auto right = coverage % 2 == 0 ? coverage - 1 : (coverage - 1) / 2;
        auto pairs = MediaCheckedArithmetic::multiply(left, right, "Startup pending interval pairs");
        if (!pairs) return R::failure(pairs.error());
        auto live = MediaCheckedArithmetic::add(pairs.value(), coverageHeads.value(),
            "Startup pending nodes, live heads and temporary move head");
        if (!live) return R::failure(live.error());
        pending = live.value();
    }
    const std::array<std::uint64_t, 4> limits{
        ordinary.value(), ordinary.value(), coverageHeads.value(), pending};
    MediaPreparedStartupIndexStorage result{};
    std::uint64_t total = 0;
    for (std::size_t index = 0; index != limits.size(); ++index) {
        if (!geometry[index].bytes) return R::failure(::media::ErrorInfo::invalidArgument(
            "Startup index requires nonempty allocator geometry"));
        auto footprint = MediaPreparationMemoryResource::allocationFootprint(
            geometry[index].bytes, geometry[index].alignment);
        if (!footprint) return R::failure(footprint.error());
        auto bytes = MediaCheckedArithmetic::multiply(limits[index], footprint.value(),
            "Startup index allocation footprint");
        if (!bytes) return R::failure(bytes.error());
        auto sum = MediaCheckedArithmetic::add(total, bytes.value(), "Startup index total footprint");
        if (!sum) return R::failure(sum.error());
        if (limits[index] > (std::numeric_limits<std::size_t>::max)() ||
            bytes.value() > (std::numeric_limits<std::size_t>::max)() ||
            sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed(
                "Startup index storage exceeds addressable memory"));
        result.allocations[index] = {geometry[index], static_cast<std::size_t>(limits[index]),
            static_cast<std::size_t>(bytes.value())};
        total = sum.value();
    }
    result.allocationBytes = static_cast<std::size_t>(total);
    return R::success(std::move(result));
}
} // namespace

::media::Result<MediaPreparedStartupIndexStorage> MediaAvStartupIndexStoragePlanner::preparedIndex(
    std::size_t capacity, MediaVideoStartupReadiness readiness)
{
    using R = ::media::Result<MediaPreparedStartupIndexStorage>;
    if (!capacity || !encodeMediaVideoStartupReadiness(readiness))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup index requires planned capacity and readiness"));
    auto geometry = MediaAvStartupStreamStore::allocationGeometry();
    if (!geometry) return R::failure(geometry.error());
    return prepare(capacity, readiness, geometry.value());
}

::media::Result<MediaAvStartupIndexStoragePlan> MediaAvStartupIndexStoragePlanner::plan(
    std::size_t capacity, MediaVideoStartupReadiness readiness,
    const MediaLineageAllocationScope& allocation)
{
    using R = ::media::Result<MediaAvStartupIndexStoragePlan>;
    if (!capacity || !encodeMediaVideoStartupReadiness(readiness))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup index requires planned capacity and readiness"));
    if (std::holds_alternative<MediaRuntimeLineageAllocation>(allocation))
        return R::success({capacity, readiness, MediaRuntimeStartupIndexStorage{}});
    if (!std::get<MediaPreparedLineageAllocation>(allocation).resource)
        return R::failure(::media::ErrorInfo::invalidArgument("Startup index requires its prepared resource owner"));
    auto prepared = preparedIndex(capacity, readiness);
    if (!prepared) return R::failure(prepared.error());
    return R::success({capacity, readiness, std::move(prepared).value()});
}

::media::Status MediaAvStartupIndexStoragePlanner::validate(const MediaAvStartupIndexStoragePlan& plan,
    std::size_t capacity, MediaVideoStartupReadiness readiness,
    const MediaLineageAllocationScope& allocation)
{
    const bool preparedScope = std::holds_alternative<MediaPreparedLineageAllocation>(allocation);
    const auto* prepared = std::get_if<MediaPreparedStartupIndexStorage>(&plan.storage);
    if (!capacity || !encodeMediaVideoStartupReadiness(readiness) || plan.capacity != capacity ||
        plan.readiness != readiness || preparedScope != (prepared != nullptr) ||
        (preparedScope && !std::get<MediaPreparedLineageAllocation>(allocation).resource))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup index plan differs from its source capacity, readiness or allocation domain"));
    if (!prepared) return ::media::Status::success();
    MediaAvStartupIndexGeometries geometry{};
    for (std::size_t i = 0; i != geometry.size(); ++i) geometry[i] = prepared->allocations[i].request;
    auto expected = prepare(capacity, readiness, geometry);
    if (!expected) return ::media::Status::failure(expected.error());
    if (prepared->allocationBytes != expected.value().allocationBytes)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup index total footprint differs from geometry"));
    for (std::size_t i = 0; i != geometry.size(); ++i)
        if (prepared->allocations[i].maximumLiveRequests != expected.value().allocations[i].maximumLiveRequests ||
            prepared->allocations[i].allocationBytes != expected.value().allocations[i].allocationBytes)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup index class footprint differs from geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
