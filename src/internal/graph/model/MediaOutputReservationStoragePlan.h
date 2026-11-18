#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
#include <optional>

namespace media::ffmpeg::graph {
struct MediaOutputReservationLayout final {
    MediaSharedAllocationGeometry ownedBatch;
    MediaSharedAllocationGeometry channelCount;
    MediaSharedAllocationGeometry channelPointer;
    MediaSharedAllocationGeometry lock;
    MediaSharedAllocationGeometry recordReference;
    MediaSharedAllocationGeometry bufferReference;
    MediaSharedAllocationGeometry character;
    MediaSharedAllocationGeometry record;
    friend bool operator==(const MediaOutputReservationLayout&,
        const MediaOutputReservationLayout&) = default;
};
// Only the planner creates a seal. Runtime has no vendor-specific layout query.
class MediaOutputReservationStoragePlan final {
public:
    const MediaLineageAllocationScope& allocation() const noexcept { return m_allocation; }
    const std::optional<MediaOutputReservationLayout>& prepared() const noexcept { return m_prepared; }
private:
    friend class MediaOutputReservationStoragePlanner;
    MediaOutputReservationStoragePlan(MediaLineageAllocationScope allocation,
        std::optional<MediaOutputReservationLayout> prepared)
        : m_allocation(std::move(allocation)), m_prepared(std::move(prepared)) {}
    MediaLineageAllocationScope m_allocation;
    std::optional<MediaOutputReservationLayout> m_prepared;
};
} // namespace media::ffmpeg::graph
