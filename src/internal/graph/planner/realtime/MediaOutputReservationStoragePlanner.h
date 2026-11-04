#pragma once
#include "internal/graph/model/MediaOutputReservationStoragePlan.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include <ranges>

namespace media::ffmpeg::graph {
class MediaOutputReservationStoragePlanner final {
public:
    static ::media::Result<MediaOutputReservationLayout> layout();
    static ::media::Result<MediaOutputReservationStoragePlan> plan(MediaLineageAllocationScope allocation);
    static ::media::Status validate(const MediaOutputReservationStoragePlan& plan,
        const MediaLineageAllocationScope& allocation);
    template<std::ranges::sized_range Counts>
    static ::media::Result<std::size_t> reservationBytes(const MediaOutputReservationLayout& layout,
        const Counts& bufferCounts, std::size_t uniqueChannels, std::size_t ownerCharacters)
    {
        auto bytes = reservationMetadataBytes(layout, std::ranges::size(bufferCounts), uniqueChannels, ownerCharacters);
        if (!bytes) return bytes;
        auto total = bytes.value();
        for (const std::size_t count : bufferCounts)
            if (auto status = addRequest(total, layout.bufferReference, count); !status)
                return ::media::Result<std::size_t>::failure(status.error());
        return ::media::Result<std::size_t>::success(total);
    }
    static ::media::Result<std::size_t> authorizationBytes(const MediaOutputReservationLayout& layout,
        std::size_t handles, std::size_t channelSlots, std::size_t uniqueChannels);
private:
    static ::media::Status addRequest(std::size_t& total,
        const MediaSharedAllocationGeometry& item, std::size_t count);
    static ::media::Result<std::size_t> reservationMetadataBytes(const MediaOutputReservationLayout& layout,
        std::size_t batches, std::size_t uniqueChannels, std::size_t ownerCharacters);
};
} // namespace media::ffmpeg::graph
