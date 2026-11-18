#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
// The decoded index has two entry classes, empty-container head facts,
// and no pending intervals. Forward pricing and the budget inverse consume
// the same declaration rather than estimating a slope from samples.
struct LinearRequests final { std::uint64_t perUnit; std::uint64_t fixed; };
std::array<LinearRequests, 4> decodedRequests(const MediaAvStartupIndexGeometries& geometry)
{
    return {{{1, geometry[0].emptyHeadRequests}, {1, geometry[1].emptyHeadRequests},
        {0, geometry[2].emptyHeadRequests}, {0, 0}}};
}
using IndexFootprints = std::array<std::size_t, 4>;

::media::Result<IndexFootprints> footprints(const MediaAvStartupIndexGeometries& geometry)
{
    using R = ::media::Result<IndexFootprints>;
    IndexFootprints result{};
    for (std::size_t i = 0; i < geometry.size(); ++i) {
        if (!geometry[i].request.bytes) return R::failure(::media::ErrorInfo::invalidArgument(
            "Startup index requires nonempty allocator geometry"));
        auto footprint = MediaPreparationMemoryResource::allocationFootprint(geometry[i].request.bytes, geometry[i].request.alignment);
        if (!footprint) return R::failure(footprint.error());
        result[i] = footprint.value();
    }
    return R::success(result);
}

::media::Result<MediaPreparedStartupIndexStorage> prepare(
    std::size_t capacity, MediaVideoStartupReadiness readiness,
    const MediaAvStartupIndexGeometries& geometry)
{
    using R = ::media::Result<MediaPreparedStartupIndexStorage>;
    const std::uint64_t count = capacity;
    const auto requestsPerClass = decodedRequests(geometry);
    std::array<std::uint64_t, 4> limits{};
    for (std::size_t i = 0; i < limits.size(); ++i) {
        auto nodes = MediaCheckedArithmetic::multiply(count, requestsPerClass[i].perUnit,
            "Startup index node count");
        if (!nodes) return R::failure(nodes.error());
        auto requests = MediaCheckedArithmetic::add(nodes.value(), requestsPerClass[i].fixed,
            "Startup index node and head count");
        if (!requests) return R::failure(requests.error());
        limits[i] = requests.value();
    }
    if (readiness == MediaVideoStartupReadiness::IntervalCoverage) {
        auto coverageHeads = MediaCheckedArithmetic::add(count,
            geometry[static_cast<std::size_t>(MediaAvStartupIndexClass::Coverage)].emptyHeadRequests,
            "Startup coverage node and head count");
        if (!coverageHeads) return R::failure(coverageHeads.error());
        // Each later interval can occur once in each earlier candidate. Divide
        // an even factor first; no N*N intermediate or sampled growth estimate.
        const auto left = count % 2 == 0 ? count / 2 : count;
        const auto right = count % 2 == 0 ? count - 1 : (count - 1) / 2;
        auto pairs = MediaCheckedArithmetic::multiply(left, right, "Startup pending interval pairs");
        if (!pairs) return R::failure(pairs.error());
        auto pendingContainers = MediaCheckedArithmetic::add(count, 1, "Startup pending containers including temporary move");
        if (!pendingContainers) return R::failure(pendingContainers.error());
        auto pendingHeads = MediaCheckedArithmetic::multiply(pendingContainers.value(),
            geometry[static_cast<std::size_t>(MediaAvStartupIndexClass::Pending)].emptyHeadRequests,
            "Startup pending container heads");
        if (!pendingHeads) return R::failure(pendingHeads.error());
        auto live = MediaCheckedArithmetic::add(pairs.value(), pendingHeads.value(),
            "Startup pending nodes, live heads and temporary move head");
        if (!live) return R::failure(live.error());
        limits[static_cast<std::size_t>(MediaAvStartupIndexClass::Coverage)] = coverageHeads.value();
        limits[static_cast<std::size_t>(MediaAvStartupIndexClass::Pending)] = live.value();
    }
    auto costs = footprints(geometry);
    if (!costs) return R::failure(costs.error());
    MediaPreparedStartupIndexStorage result{};
    std::uint64_t total = 0;
    for (std::size_t index = 0; index != limits.size(); ++index) {
        auto bytes = MediaCheckedArithmetic::multiply(limits[index], costs.value()[index],
            "Startup index allocation footprint");
        if (!bytes) return R::failure(bytes.error());
        auto sum = MediaCheckedArithmetic::add(total, bytes.value(), "Startup index total footprint");
        if (!sum) return R::failure(sum.error());
        if (limits[index] > (std::numeric_limits<std::size_t>::max)() ||
            bytes.value() > (std::numeric_limits<std::size_t>::max)() ||
            sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed(
                "Startup index storage exceeds addressable memory"));
        result.allocations[index] = {geometry[index].request, static_cast<std::size_t>(limits[index]),
            static_cast<std::size_t>(bytes.value()), geometry[index].emptyHeadRequests};
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

::media::Result<std::size_t> MediaAvStartupIndexStoragePlanner::maximumPreparedDisplayCapacity(
    std::size_t availableBytes)
{
    using R = ::media::Result<std::size_t>;
    auto geometry = MediaAvStartupStreamStore::allocationGeometry();
    if (!geometry) return R::failure(geometry.error());
    auto costs = footprints(geometry.value());
    if (!costs) return R::failure(costs.error());
    std::uint64_t perUnit = 0;
    std::uint64_t fixed = 0;
    const auto requestsPerClass = decodedRequests(geometry.value());
    for (std::size_t i = 0; i < requestsPerClass.size(); ++i) {
        auto variableBytes = MediaCheckedArithmetic::multiply(requestsPerClass[i].perUnit, costs.value()[i],
            "Decoded index per-unit footprint");
        auto fixedBytes = MediaCheckedArithmetic::multiply(requestsPerClass[i].fixed, costs.value()[i],
            "Decoded index fixed footprint");
        if (!variableBytes || !fixedBytes) return R::failure(!variableBytes ? variableBytes.error() : fixedBytes.error());
        auto variableSum = MediaCheckedArithmetic::add(perUnit, variableBytes.value(), "Decoded index per-unit total");
        auto fixedSum = MediaCheckedArithmetic::add(fixed, fixedBytes.value(), "Decoded index fixed total");
        if (!variableSum || !fixedSum) return R::failure(!variableSum ? variableSum.error() : fixedSum.error());
        perUnit = variableSum.value();
        fixed = fixedSum.value();
    }
    if (!perUnit) return R::failure(::media::ErrorInfo::invalidArgument("Decoded index has no per-unit storage"));
    if (availableBytes < fixed) return R::success(0);
    const auto capacity = (static_cast<std::uint64_t>(availableBytes) - fixed) / perUnit;
    // The existing decoded startup contract uses a positive signed-int count.
    return R::success(static_cast<std::size_t>((std::min)(capacity,
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))));
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
    auto geometry = MediaAvStartupStreamStore::allocationGeometry();
    if (!geometry) return ::media::Status::failure(geometry.error());
    auto expected = prepare(capacity, readiness, geometry.value());
    if (!expected) return ::media::Status::failure(expected.error());
    if (prepared->allocationBytes != expected.value().allocationBytes)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup index total footprint differs from geometry"));
    for (std::size_t i = 0; i != geometry.value().size(); ++i)
        if (prepared->allocations[i] != expected.value().allocations[i])
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup index class footprint differs from geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
