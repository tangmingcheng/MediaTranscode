#pragma once
#include "internal/graph/model/MediaAvStartupIndexStoragePlan.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"

namespace media::ffmpeg::graph {
struct MediaAvStartupHashGeometry final {
    MediaAvStartupIndexGeometry node;
    MediaAvStartupIndexGeometry initialBuckets;
    MediaAvStartupIndexGeometry finalBuckets;
    std::size_t initialBucketCount;
    std::size_t bucketCount;
    float maximumLoadFactor;
    bool operator==(const MediaAvStartupHashGeometry&) const noexcept = default;
};
struct MediaAvStartupHashStoragePlan final {
    MediaAvStartupHashGeometry geometry;
    MediaAvStartupIndexAllocationPlan nodes;
    std::size_t allocationBytes;
    bool operator==(const MediaAvStartupHashStoragePlan&) const noexcept = default;
};
struct MediaPreparedStartupGenerationStorage final {
    MediaSharedAllocationGeometry sharedObject;
    std::size_t sharedObjectAllocationBytes;
    std::array<MediaAvStartupHashStoragePlan, 2> hashes;
    std::size_t allocationBytes;
    bool operator==(const MediaPreparedStartupGenerationStorage&) const noexcept = default;
};
struct MediaAvStartupGenerationStoragePlan final {
    std::size_t maximumElements;
    std::variant<MediaRuntimeStartupIndexStorage, MediaPreparedStartupGenerationStorage> storage;
};
} // namespace media::ffmpeg::graph
