#pragma once
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include <array>
#include <cstddef>
#include <variant>

namespace media::ffmpeg::graph {

// These are allocator request classes, not logical media capacity classes.
enum class MediaAvStartupIndexClass : std::size_t { Arrival, Presentation, Coverage, Pending };
struct MediaAvStartupIndexGeometry final {
    std::size_t bytes;
    std::size_t alignment;
    bool operator==(const MediaAvStartupIndexGeometry&) const noexcept = default;
};
struct MediaAvStartupIndexContainerGeometry final {
    MediaAvStartupIndexGeometry request;
    std::size_t emptyHeadRequests;
    bool operator==(const MediaAvStartupIndexContainerGeometry&) const noexcept = default;
};
using MediaAvStartupIndexGeometries = std::array<MediaAvStartupIndexContainerGeometry, 4>;
struct MediaAvStartupIndexAllocationPlan final {
    MediaAvStartupIndexGeometry request;
    std::size_t maximumLiveRequests;
    std::size_t allocationBytes;
    std::size_t emptyHeadRequests;
    bool operator==(const MediaAvStartupIndexAllocationPlan&) const noexcept = default;
};
struct MediaRuntimeStartupIndexStorage final {};
struct MediaPreparedStartupIndexStorage final {
    std::array<MediaAvStartupIndexAllocationPlan, 4> allocations;
    std::size_t allocationBytes;
};
struct MediaAvStartupIndexStoragePlan final {
    std::size_t capacity;
    MediaVideoStartupReadiness readiness;
    std::variant<MediaRuntimeStartupIndexStorage, MediaPreparedStartupIndexStorage> storage;
};

} // namespace media::ffmpeg::graph
