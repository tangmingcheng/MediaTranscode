#pragma once
#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRouteMediaPlanner.h"
namespace media::ffmpeg::graph {
// Candidate facts have no allocated identities, sealed source or graph options.
struct MediaCompositionSourceResidenceFacts final {
    MediaStartupRoutePlan route;
    std::uint64_t videoBatchUnits;
    std::optional<std::uint64_t> audioBatchUnits;
    std::uint64_t videoAcquiringCapacity;
    std::optional<std::uint64_t> audioAcquiringCapacity;
    std::uint64_t videoDescriptorCapacity;
    std::optional<std::uint64_t> audioDescriptorCapacity;
    std::optional<std::uint64_t> filterRetainedInputs;
};
struct MediaCompositionResidenceFacts final {
    std::span<const MediaCompositionSourceResidenceFacts> sources;
    std::uint64_t videoEncoderRetainedFrames;
    std::uint64_t audioEncoderRetainedFrames;
    std::uint64_t aggregateReferences;
};
struct MediaCompositionStartupNodeResidence final {
    std::size_t position;
    std::size_t coordinator;
    MediaPayloadKind videoPayload;
    MediaStartupRouteMediaUnits units;
};
struct MediaCompositionStartupEventResidence final {
    std::size_t connection;
    std::size_t coordinator;
    MediaPayloadKind videoPayload;
    MediaStartupRouteEventUnits units;
};
struct MediaCompositionReferenceResidencePlan final {
    // Keep the allocator alive until every nested position catalog is destroyed.
    const std::shared_ptr<MediaPreparationMemoryResource> resource;
    explicit MediaCompositionReferenceResidencePlan(std::shared_ptr<MediaPreparationMemoryResource> owner)
        : resource(std::move(owner)), nodeReferences(*resource), startupNodes(*resource),
          startupEvents(*resource), aggregate(resource, {}, 0,
              MediaPayloadResidenceOwnerKind::Shared, std::nullopt, std::nullopt) {}
    // Empty means a typed startup/shared owner replaces ordinary node retention.
    MediaPlanningCatalog<std::optional<std::uint64_t>> nodeReferences;
    MediaPlanningCatalog<MediaCompositionStartupNodeResidence> startupNodes;
    MediaPlanningCatalog<MediaCompositionStartupEventResidence> startupEvents;
    MediaPayloadResidenceOwner aggregate;
};
class MediaCompositionPayloadResidencePlanner final {
public:
    // Rebind one catalog owner from the same fixed topology/domain. The caller
    // must preserve topology shape, slot presence, and producer reachability;
    // this checks the stored slot identity, not an arbitrary replacement graph.
    // Success changes only the count and allocates no storage.
    static ::media::Status rebindOwner(
        const MediaCompositionGraphTopology& topology, const MediaCompositionReferenceResidencePlan& reference,
        MediaStreamKind stream, MediaPayloadKind payload, MediaPayloadResidenceOwner& owner);
    // Same fixed topology/role mapping as compile. Recomputes quantities using
    // the same planners without allocating or resizing on success. Identity or
    // optional-member changes fail; discard the entire candidate after failure.
    static ::media::Status rebind(
        const MediaCompositionGraphTopology& topology, const MediaCompositionResidenceFacts& facts,
        MediaCompositionReferenceResidencePlan& reference);
    static ::media::Result<MediaCompositionReferenceResidencePlan> compile(
        const MediaCompositionGraphTopology& topology,const MediaCompositionResidenceFacts& facts);
    static ::media::Result<MediaPayloadResidenceProjection> plan(
        const MediaCompositionGraphTopology& topology,const MediaCompositionReferenceResidencePlan& reference,
        std::size_t producerPosition,MediaStreamKind stream,MediaPayloadKind payload,
        MediaPayloadResidenceSelection selection);
};
}
