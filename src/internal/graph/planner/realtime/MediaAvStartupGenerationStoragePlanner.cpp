#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
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
::media::Result<MediaPreparedStartupGenerationStorage> prepare(
    std::size_t maximum, const std::array<MediaAvStartupHashGeometry, 2>& geometry,
    const MediaSharedAllocationGeometry& sharedObject)
{
    using R = ::media::Result<MediaPreparedStartupGenerationStorage>;
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
        if (!facts.initialBucketCount || !facts.bucketCount || facts.maximumLoadFactor != 1.0f ||
            facts.bucketCount < maximum || !facts.node.bytes || !facts.finalBuckets.bytes ||
            (facts.initialBuckets && (!facts.initialBuckets->bytes ||
                facts.initialBuckets->alignment != facts.finalBuckets.alignment)))
            return R::failure(::media::ErrorInfo::unsupported("Startup hash geometry differs from audited node/bucket layout"));
        auto nodes = MediaCheckedArithmetic::add(maximum, facts.initialNodeRequests, "Startup hash elements and empty nodes");
        if (!nodes) return R::failure(nodes.error());
        auto nodeFootprint = MediaPreparationMemoryResource::allocationFootprint(facts.node.bytes, facts.node.alignment);
        auto finalFootprint = MediaPreparationMemoryResource::allocationFootprint(facts.finalBuckets.bytes, facts.finalBuckets.alignment);
        if (!nodeFootprint || !finalFootprint)
            return R::failure(!nodeFootprint ? nodeFootprint.error() : finalFootprint.error());
        std::size_t initialFootprint = 0;
        if (facts.initialBuckets) {
            auto footprint = MediaPreparationMemoryResource::allocationFootprint(
                facts.initialBuckets->bytes, facts.initialBuckets->alignment);
            if (!footprint) return R::failure(footprint.error());
            initialFootprint = footprint.value();
        }
        auto nodeBytes = MediaCheckedArithmetic::multiply(nodes.value(), nodeFootprint.value(), "Startup hash node footprint");
        if (!nodeBytes) return R::failure(nodeBytes.error());
        auto retained = MediaCheckedArithmetic::add(nodeBytes.value(), finalFootprint.value(), "Startup hash sealed storage");
        if (!retained) return R::failure(retained.error());
        auto emptyNodes = MediaCheckedArithmetic::multiply(facts.initialNodeRequests, nodeFootprint.value(), "Startup hash empty nodes");
        if (!emptyNodes) return R::failure(emptyNodes.error());
        auto preparing = MediaCheckedArithmetic::add(emptyNodes.value(), initialFootprint, "Startup hash initial storage");
        if (!preparing) return R::failure(preparing.error());
        if (!facts.initialBuckets || facts.bucketCount != facts.initialBucketCount) {
            preparing = MediaCheckedArithmetic::add(preparing.value(), finalFootprint.value(), "Startup hash rehash overlap");
            if (!preparing) return R::failure(preparing.error());
        }
        const auto peak = (std::max)(retained.value(), preparing.value());
        auto sum = MediaCheckedArithmetic::add(total, peak, "Startup generation shared object and hash total footprint");
        if (!sum) return R::failure(sum.error());
        if (nodes.value() > (std::numeric_limits<std::size_t>::max)() || sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup generation storage footprint exceeds addressable memory"));
        result.hashes[i] = {facts, {facts.node, static_cast<std::size_t>(nodes.value()),
            static_cast<std::size_t>(nodeBytes.value()), facts.initialNodeRequests}, static_cast<std::size_t>(peak)};
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
    auto geometry = MediaAvStartupGenerationState::allocationGeometry(maximum.value());
    if (!geometry) return R::failure(geometry.error());
    auto sharedObject = MediaAvStartupGenerationState::sharedAllocationGeometry();
    if (!sharedObject) return R::failure(sharedObject.error());
    return prepare(maximum.value(), geometry.value(), sharedObject.value());
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
    auto geometry = MediaAvStartupGenerationState::allocationGeometry(maximum.value());
    if (!geometry) return ::media::Status::failure(geometry.error());
    auto sharedObject = MediaAvStartupGenerationState::sharedAllocationGeometry();
    if (!sharedObject) return ::media::Status::failure(sharedObject.error());
    auto expected = prepare(maximum.value(), geometry.value(), sharedObject.value());
    if (!expected) return ::media::Status::failure(expected.error());
    if (*prepared != expected.value())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup generation node limits or footprints differ from planned geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
