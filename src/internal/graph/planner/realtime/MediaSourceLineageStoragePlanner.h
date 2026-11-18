#pragma once
#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include <array>

namespace media::ffmpeg::graph {
enum class MediaSourceLineageAllocationFamily { AccessUnit, Display, PacketWrapper, FrameWrapper };
enum class MediaSourceLineageRootRole { PacketHolder, FrameHolder, Construction, Registry, PendingLineage, ResolvedLineage, PreparedPrefix };
struct MediaSourceLineageAllocationRoot final {
    std::size_t creatorPosition;
    MediaSourceLineageRootRole role;
    std::optional<MediaPayloadResidenceOwner> holder;
    std::uint64_t maximumReferences;
};
struct MediaSourceLineageAllocationCost final {
    MediaSourceLineageAllocationCost(std::pmr::memory_resource& resource, MediaSourceLineageAllocationFamily kind)
        : family(kind), rootIndices(resource), maximumAllocations(0), preparationBytes(0) {}
    MediaSourceLineageAllocationFamily family;
    MediaPlanningCatalog<std::size_t> rootIndices;
    std::uint64_t maximumAllocations;
    std::uint64_t preparationBytes;
};
// Only engine objects allocated through the source preparation resource.
// Registry storage, raw media wrappers and FFmpeg private memory are excluded.
struct MediaSourceLineageStoragePlan final {
    const std::shared_ptr<MediaPreparationMemoryResource> resource;
    MediaSourceLineageStoragePlan(std::shared_ptr<MediaPreparationMemoryResource> owner, std::size_t source)
        : resource(std::move(owner)), sourceIndex(source), roots(*resource),
          allocations{{{*resource, MediaSourceLineageAllocationFamily::AccessUnit},
              {*resource, MediaSourceLineageAllocationFamily::Display},
              {*resource, MediaSourceLineageAllocationFamily::PacketWrapper},
              {*resource, MediaSourceLineageAllocationFamily::FrameWrapper}}}, preparationBytes(0) {}
    std::size_t sourceIndex;
    // Family membership refers to this owned catalog. Indices remain valid
    // across vector growth and moving the complete planning product.
    MediaPlanningCatalog<MediaSourceLineageAllocationRoot> roots;
    std::array<MediaSourceLineageAllocationCost, 4> allocations;
    std::uint64_t preparationBytes;
};
class MediaSourceLineageStoragePlanner final {
public:
    // Capacity-only rebinding of a catalog from the same fixed topology and
    // projection domain. No successful-path allocations. On failure discard
    // the candidate: some root counts may already have changed; do not resume
    // a search from it. Structural identities and memberships are untouched.
    static ::media::Status rebind(MediaSourceLineageStoragePlan& plan,
        const MediaCompositionGraphTopology& topology, const MediaCompositionReferenceResidencePlan& reference,
        std::uint64_t registryCapacity, std::uint64_t preparedPrefixCapacity);
    // Reuses the existing root catalog and family memberships. Successful
    // reevaluation allocates no storage and replaces only numeric costs.
    // On failure the previously calculated costs remain unchanged.
    static ::media::Status evaluate(MediaSourceLineageStoragePlan& plan);
    static ::media::Result<MediaSourceLineageStoragePlan> plan(
        const MediaCompositionGraphTopology& topology,
        const MediaCompositionReferenceResidencePlan& reference,
        std::size_t sourceIndex, std::uint64_t registryCapacity, std::uint64_t preparedPrefixCapacity,
        MediaHardwareTransferDirection transferDirection);
};
}
