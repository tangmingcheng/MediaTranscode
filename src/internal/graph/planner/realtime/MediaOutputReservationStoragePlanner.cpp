#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {
using Bytes = ::media::Result<std::size_t>;
::media::Status MediaOutputReservationStoragePlanner::addRequest(std::size_t& total, const MediaSharedAllocationGeometry& item, std::size_t count)
{
    if (item.count != 1 || !item.bytes)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Reservation layout requires a single nonempty allocation request"));
    if (!count) return ::media::Status::success();
    auto bytes = MediaCheckedArithmetic::multiply(item.bytes, count, "reservation array request");
    if (!bytes) return ::media::Status::failure(bytes.error());
    if (bytes.value() > (std::numeric_limits<std::size_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Reservation request exceeds addressable storage"));
    auto footprint = MediaPreparationMemoryResource::allocationFootprint(static_cast<std::size_t>(bytes.value()), item.alignment);
    if (!footprint) return ::media::Status::failure(footprint.error());
    auto sum = MediaCheckedArithmetic::add(total, footprint.value(), "reservation request footprint");
    if (!sum) return ::media::Status::failure(sum.error());
    if (sum.value() > (std::numeric_limits<std::size_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Reservation footprint exceeds addressable storage"));
    total = static_cast<std::size_t>(sum.value());
    return ::media::Status::success();
}
::media::Result<MediaOutputReservationLayout> MediaOutputReservationStoragePlanner::layout()
{ return MediaReservedOutputTransaction::allocationLayout(); }
::media::Result<MediaOutputReservationStoragePlan> MediaOutputReservationStoragePlanner::plan(MediaLineageAllocationScope allocation)
{
    using R = ::media::Result<MediaOutputReservationStoragePlan>;
    if (!MediaLineageAllocator<std::byte>{allocation}.resource())
        return R::failure(::media::ErrorInfo::invalidArgument("Reservation storage requires its allocation owner"));
    if (std::holds_alternative<MediaRuntimeLineageAllocation>(allocation))
        return R::success(MediaOutputReservationStoragePlan{std::move(allocation), std::nullopt});
    auto actual = layout();
    if (!actual) return R::failure(actual.error());
    return R::success(MediaOutputReservationStoragePlan{std::move(allocation), std::move(actual).value()});
}
::media::Status MediaOutputReservationStoragePlanner::validate(const MediaOutputReservationStoragePlan& plan,
    const MediaLineageAllocationScope& allocation)
{
    if (plan.allocation().index() != allocation.index() ||
        plan.prepared().has_value() != std::holds_alternative<MediaPreparedLineageAllocation>(allocation) ||
        !MediaLineageAllocator<std::byte>{allocation}.resource() ||
        MediaLineageAllocator<std::byte>{allocation}.resource() != MediaLineageAllocator<std::byte>{plan.allocation()}.resource())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Reservation storage differs from its planned allocation domain"));
    if (const auto* expected = std::get_if<MediaPreparedLineageAllocation>(&allocation)) {
        const auto& original = std::get<MediaPreparedLineageAllocation>(plan.allocation());
        if (expected->resource.owner_before(original.resource) || original.resource.owner_before(expected->resource))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Reservation storage differs from its planned allocation owner"));
    }
    return ::media::Status::success();
}
Bytes MediaOutputReservationStoragePlanner::reservationMetadataBytes(const MediaOutputReservationLayout& layout,
    std::size_t batches, std::size_t uniqueChannels, std::size_t ownerCharacters)
{
    if (!uniqueChannels || uniqueChannels > batches)
        return Bytes::failure(::media::ErrorInfo::invalidArgument("Reservation storage has an invalid batch/channel shape"));
    std::size_t total = 0;
    for (const auto& item : {layout.ownedBatch, layout.channelCount})
        if (auto s = addRequest(total, item, batches); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.channelPointer, uniqueChannels); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.lock, uniqueChannels); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.lock, uniqueChannels); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.character, ownerCharacters); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.record, 1); !s) return Bytes::failure(s.error());
    return Bytes::success(total);
}
Bytes MediaOutputReservationStoragePlanner::authorizationBytes(const MediaOutputReservationLayout& layout,
    std::size_t handles, std::size_t channelSlots, std::size_t uniqueChannels)
{
    if (!handles || !uniqueChannels || uniqueChannels > channelSlots)
        return Bytes::failure(::media::ErrorInfo::invalidArgument("Authorization storage has an invalid handle/channel shape"));
    std::size_t total = 0;
    if (auto s = addRequest(total, layout.recordReference, handles); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.lock, handles); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.channelPointer, channelSlots); !s) return Bytes::failure(s.error());
    if (auto s = addRequest(total, layout.lock, uniqueChannels); !s) return Bytes::failure(s.error());
    return Bytes::success(total);
}
} // namespace media::ffmpeg::graph
