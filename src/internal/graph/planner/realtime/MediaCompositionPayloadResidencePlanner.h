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
    // Empty means a typed startup/shared owner replaces ordinary node retention.
    std::vector<std::optional<std::uint64_t>> nodeReferences;
    std::vector<MediaCompositionStartupNodeResidence> startupNodes;
    std::vector<MediaCompositionStartupEventResidence> startupEvents;
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
    static ::media::Result<MediaCompositionReferenceResidencePlan> compile(
        const MediaCompositionGraphTopology& topology,const MediaCompositionResidenceFacts& facts);
    static ::media::Result<MediaPayloadResidenceProjection> plan(
        const MediaCompositionGraphTopology& topology,const MediaCompositionReferenceResidencePlan& reference,
        std::size_t producerPosition,MediaStreamKind stream,MediaPayloadKind payload,
        MediaPayloadResidenceSelection selection);
};
}
