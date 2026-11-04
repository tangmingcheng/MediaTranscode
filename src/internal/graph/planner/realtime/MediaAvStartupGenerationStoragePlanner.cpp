#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <bit>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::size_t> capacity(std::size_t video, std::size_t audio)
{
    using R = ::media::Result<std::size_t>;
    if (!video) return R::failure(::media::ErrorInfo::invalidArgument("Startup generation storage requires video capacity"));
    auto count = MediaCheckedArithmetic::add(video, audio, "Startup generation element capacity");
    if (!count) return R::failure(count.error());
    if (count.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Startup generation capacity exceeds addressable memory"));
    return R::success(static_cast<std::size_t>(count.value()));
}
::media::Result<std::size_t> bucketCount(std::size_t maximum)
{
    using R = ::media::Result<std::size_t>;
    // Audited xhash minimum, not a media policy default. Probe independently
    // reads back the actual initial count and default maximum load factor.
    const auto required = (std::max)(std::size_t{8}, maximum);
    constexpr auto largestPower = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    if (required > largestPower)
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash power-of-two capacity overflows"));
    const auto buckets = std::bit_ceil(required);
    auto cells = MediaCheckedArithmetic::multiply(buckets, 2, "Startup hash bucket cells");
    if (!cells || cells.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Startup hash bucket cells exceed addressable memory"));
    return R::success(buckets);
}
::media::Result<MediaPreparedStartupGenerationStorage> prepare(
    std::size_t maximum, std::size_t buckets, const std::array<MediaAvStartupHashGeometry, 2>& geometry,
    const MediaSharedAllocationGeometry& sharedObject)
{
    using R = ::media::Result<MediaPreparedStartupGenerationStorage>;
    auto nodes = MediaCheckedArithmetic::add(maximum, 1, "Startup hash nodes including sentinel");
    if (!nodes) return R::failure(nodes.error());
    if (sharedObject.count != 1 || !sharedObject.bytes)
        return R::failure(::media::ErrorInfo::invalidArgument("Startup generation requires one combined shared object/control allocation"));
    auto fixed = MediaPreparationMemoryResource::allocationFootprint(sharedObject.bytes, sharedObject.alignment);
    if (!fixed) return R::failure(fixed.error());
    MediaPreparedStartupGenerationStorage result{};
    result.sharedObject = sharedObject;
    result.sharedObjectAllocationBytes = fixed.value();
    std::uint64_t total = fixed.value();
    for (std::size_t i = 0; i != result.hashes.size(); ++i) {
        const auto& facts = geometry[i];
        if (facts.initialBucketCount != 8 || facts.bucketCount != buckets ||
            facts.maximumLoadFactor != 1.0f || !facts.node.bytes ||
            !facts.initialBuckets.bytes || facts.initialBuckets.bytes % 16 != 0 ||
            facts.initialBuckets.alignment != facts.finalBuckets.alignment)
            return R::failure(::media::ErrorInfo::unsupported("Startup hash geometry differs from audited node/bucket layout"));
        auto cells = MediaCheckedArithmetic::multiply(buckets, 2, "Startup hash final cells");
        if (!cells) return R::failure(cells.error());
        auto bucketBytes = MediaCheckedArithmetic::multiply(cells.value(), facts.initialBuckets.bytes / 16,
            "Startup hash final bucket request");
        if (!bucketBytes) return R::failure(bucketBytes.error());
        if (bucketBytes.value() != facts.finalBuckets.bytes)
            return R::failure(::media::ErrorInfo::invalidArgument("Startup hash bucket request differs from cell geometry"));
        auto nodeFootprint = MediaPreparationMemoryResource::allocationFootprint(facts.node.bytes, facts.node.alignment);
        auto initialFootprint = MediaPreparationMemoryResource::allocationFootprint(facts.initialBuckets.bytes, facts.initialBuckets.alignment);
        auto finalFootprint = MediaPreparationMemoryResource::allocationFootprint(facts.finalBuckets.bytes, facts.finalBuckets.alignment);
        if (!nodeFootprint || !initialFootprint || !finalFootprint)
            return R::failure(!nodeFootprint ? nodeFootprint.error() : !initialFootprint ? initialFootprint.error() : finalFootprint.error());
        auto nodeBytes = MediaCheckedArithmetic::multiply(nodes.value(), nodeFootprint.value(), "Startup hash node footprint");
        if (!nodeBytes) return R::failure(nodeBytes.error());
        auto retained = MediaCheckedArithmetic::add(nodeBytes.value(), finalFootprint.value(), "Startup hash sealed storage");
        if (!retained) return R::failure(retained.error());
        auto preparing = MediaCheckedArithmetic::add(nodeFootprint.value(), initialFootprint.value(), "Startup hash initial storage");
        if (!preparing) return R::failure(preparing.error());
        if (facts.bucketCount != facts.initialBucketCount) {
            preparing = MediaCheckedArithmetic::add(preparing.value(), finalFootprint.value(), "Startup hash rehash overlap");
            if (!preparing) return R::failure(preparing.error());
        }
        const auto peak = (std::max)(retained.value(), preparing.value());
        auto sum = MediaCheckedArithmetic::add(total, peak, "Startup generation shared object and hash total footprint");
        if (!sum) return R::failure(sum.error());
        if (nodes.value() > (std::numeric_limits<std::size_t>::max)() || sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup generation storage footprint exceeds addressable memory"));
        result.hashes[i] = {facts, {facts.node, static_cast<std::size_t>(nodes.value()),
            static_cast<std::size_t>(nodeBytes.value())}, static_cast<std::size_t>(peak)};
        total = sum.value();
    }
    result.allocationBytes = static_cast<std::size_t>(total);
    return R::success(std::move(result));
}
} // namespace

::media::Result<MediaPreparedStartupGenerationStorage> MediaAvStartupIndexStoragePlanner::preparedGeneration(
    std::size_t videoCapacity, std::size_t audioCapacity)
{
    using R = ::media::Result<MediaPreparedStartupGenerationStorage>;
    auto maximum = capacity(videoCapacity, audioCapacity);
    if (!maximum) return R::failure(maximum.error());
    auto buckets = bucketCount(maximum.value());
    if (!buckets) return R::failure(buckets.error());
    auto geometry = MediaAvStartupGenerationState::allocationGeometry(buckets.value());
    if (!geometry) return R::failure(geometry.error());
    auto sharedObject = MediaAvStartupGenerationState::sharedAllocationGeometry();
    if (!sharedObject) return R::failure(sharedObject.error());
    return prepare(maximum.value(), buckets.value(), geometry.value(), sharedObject.value());
}

::media::Result<MediaAvStartupGenerationStoragePlan> MediaAvStartupIndexStoragePlanner::planGeneration(
    std::size_t videoCapacity, std::size_t audioCapacity, const MediaLineageAllocationScope& allocation)
{
    using R = ::media::Result<MediaAvStartupGenerationStoragePlan>;
    auto maximum = capacity(videoCapacity, audioCapacity);
    if (!maximum) return R::failure(maximum.error());
    if (std::holds_alternative<MediaRuntimeLineageAllocation>(allocation))
        return R::success({maximum.value(), MediaRuntimeStartupIndexStorage{}});
    const auto& owner = std::get<MediaPreparedLineageAllocation>(allocation).resource;
    if (!owner) return R::failure(::media::ErrorInfo::invalidArgument("Startup generation storage requires its prepared owner"));
    auto result = preparedGeneration(videoCapacity, audioCapacity);
    if (!result) return R::failure(result.error());
    return R::success({maximum.value(), std::move(result).value()});
}

::media::Status MediaAvStartupIndexStoragePlanner::validateGeneration(
    const MediaAvStartupGenerationStoragePlan& plan, std::size_t videoCapacity,
    std::size_t audioCapacity, const MediaLineageAllocationScope& allocation)
{
    auto maximum = capacity(videoCapacity, audioCapacity);
    if (!maximum) return ::media::Status::failure(maximum.error());
    const auto* prepared = std::get_if<MediaPreparedStartupGenerationStorage>(&plan.storage);
    const auto* scope = std::get_if<MediaPreparedLineageAllocation>(&allocation);
    if (plan.maximumElements != maximum.value() || (prepared != nullptr) != (scope != nullptr) ||
        (scope && !scope->resource))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup generation plan differs from its source capacity or allocation domain"));
    if (!prepared) return ::media::Status::success();
    auto buckets = bucketCount(maximum.value());
    if (!buckets) return ::media::Status::failure(buckets.error());
    const std::array<MediaAvStartupHashGeometry, 2> geometry{
        prepared->hashes[0].geometry, prepared->hashes[1].geometry};
    auto sharedObject = MediaAvStartupGenerationState::sharedAllocationGeometry();
    if (!sharedObject) return ::media::Status::failure(sharedObject.error());
    auto expected = prepare(maximum.value(), buckets.value(), geometry, sharedObject.value());
    if (!expected) return ::media::Status::failure(expected.error());
    if (*prepared != expected.value())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup generation node limits or footprints differ from planned geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
