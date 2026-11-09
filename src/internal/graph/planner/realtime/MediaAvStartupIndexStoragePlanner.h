#pragma once
#include "internal/graph/model/MediaAvStartupIndexStoragePlan.h"
#include "internal/graph/model/MediaAvStartupGenerationStoragePlan.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

namespace media::ffmpeg::graph {
class MediaAvStartupIndexStoragePlanner final {
public:
    // Pure Prepared-domain costs. They inspect audited allocator types without
    // creating a resource, graph, or candidate container.
    static ::media::Result<MediaPreparedStartupIndexStorage> preparedIndex(
        std::size_t capacity, MediaVideoStartupReadiness readiness);
    // Exact inverse of decoded prepared-index pricing, capped by the existing
    // signed-int startup count domain. Zero means no positive count fits.
    // This is an index-only bound, not complete-source admission.
    static ::media::Result<std::size_t> maximumPreparedDisplayCapacity(std::size_t availableBytes);
    static ::media::Result<MediaPreparedStartupGenerationStorage> preparedGeneration(
        std::size_t videoCapacity, std::size_t audioCapacity);
    static ::media::Status validateGeneration(const MediaAvStartupGenerationStoragePlan& plan,
        std::size_t videoCapacity, std::size_t audioCapacity, const MediaLineageAllocationScope& allocation);
    static ::media::Result<MediaAvStartupGenerationStoragePlan> planGeneration(
        std::size_t videoCapacity, std::size_t audioCapacity,
        const MediaLineageAllocationScope& allocation);
    static ::media::Result<MediaAvStartupIndexStoragePlan> plan(std::size_t capacity,
        MediaVideoStartupReadiness readiness, const MediaLineageAllocationScope& allocation);
    static ::media::Status validate(const MediaAvStartupIndexStoragePlan& plan,
        std::size_t capacity, MediaVideoStartupReadiness readiness,
        const MediaLineageAllocationScope& allocation);
};
} // namespace media::ffmpeg::graph
