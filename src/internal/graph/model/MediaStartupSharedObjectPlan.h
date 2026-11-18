#pragma once
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
#include <optional>

namespace media::ffmpeg::graph {
enum class MediaStartupSharedObjectRole { Envelope, Release, ReleaseTransaction, ControlTransaction };
struct MediaPreparedStartupSharedObject final {
    MediaSharedAllocationGeometry geometry;
    std::size_t allocationBytes;
};
// Only the planner seals the actual allocator request. Copies retain that seal.
// This is one object cost, not a total simultaneous-holder admission.
class MediaStartupSharedObjectPlan final {
public:
    MediaStartupSharedObjectPlan(const MediaStartupSharedObjectPlan&) = default;
    MediaStartupSharedObjectPlan(MediaStartupSharedObjectPlan&&) noexcept = default;
    MediaStartupSharedObjectPlan& operator=(const MediaStartupSharedObjectPlan&) = default;
    MediaStartupSharedObjectPlan& operator=(MediaStartupSharedObjectPlan&&) noexcept = default;
    MediaStartupSharedObjectRole role() const noexcept { return m_role; }
    const MediaLineageAllocationScope& allocation() const noexcept { return m_allocation; }
    const std::optional<MediaPreparedStartupSharedObject>& prepared() const noexcept { return m_prepared; }
private:
    friend class MediaStartupSharedObjectPlanner;
    MediaStartupSharedObjectPlan(MediaStartupSharedObjectRole role, MediaLineageAllocationScope allocation,
        std::optional<MediaPreparedStartupSharedObject> prepared)
        : m_role(role), m_allocation(std::move(allocation)), m_prepared(std::move(prepared)) {}
    MediaStartupSharedObjectRole m_role;
    MediaLineageAllocationScope m_allocation;
    std::optional<MediaPreparedStartupSharedObject> m_prepared;
};
} // namespace media::ffmpeg::graph
