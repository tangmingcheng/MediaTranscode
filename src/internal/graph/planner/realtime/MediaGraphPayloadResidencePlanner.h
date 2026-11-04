#pragma once

#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

namespace media::ffmpeg::graph {

// A projection for one stream/payload domain. Indices are topology positions,
// not fabricated graph nodes. An owner is charged once when any retaining
// position is reachable. Edge owners use their source position.
struct MediaPayloadResidenceConnection final {
    std::size_t from;
    std::size_t to;
};
enum class MediaPayloadResidenceOwnerKind {
    Node, Edge, StartupNode, StartupEdge, Shared
};
struct MediaPayloadResidenceOwner final {
    std::vector<std::size_t> retainingPositions;
    std::uint64_t maximumReferences;
    MediaPayloadResidenceOwnerKind kind;
    // Index in the complete topology, not the filtered media connections.
    std::optional<std::size_t> connection;
};
struct MediaPayloadResidenceTopology final {
    std::size_t nodeCount;
    std::vector<MediaPayloadResidenceConnection> connections;
    std::vector<MediaPayloadResidenceOwner> owners;
};
struct MediaPayloadResidenceProjection final {
    std::vector<std::size_t> reachablePositions;
    std::uint64_t maximumRetainedObjects;
    std::vector<MediaPayloadResidenceOwner> contributions;
};

struct MediaGraphPayloadResidenceContribution final {
    std::vector<MediaNodeId> retainingNodes;
    std::uint64_t maximumReferences;
    MediaPayloadResidenceOwnerKind kind;
    std::optional<MediaEdgeId> edge;
};
struct MediaGraphPayloadResidence final {
    std::vector<MediaNodeId> reachableNodes;
    std::uint64_t maximumRetainedObjects;
    std::vector<MediaGraphPayloadResidenceContribution> contributions;
};

struct MediaGraphPayloadResidenceFacts final {
    std::span<const MediaFinalGraphResourceLedgerEntry> storage;
    std::span<const MediaStartupEventMediaBound> startupEvents;
    std::span<const MediaStartupNodeMediaBound> startupNodes;
};

// Bounds references to one producer's allocation, not frame cadence or the
// decoder's internal surface pool. Startup wrappers use the validated final
// graph products; ordinary storage remains a conservative reference bound.
class MediaGraphPayloadResidencePlanner final {
public:
    static ::media::Result<MediaPayloadResidenceProjection> plan(
        const MediaPayloadResidenceTopology& topology, std::size_t producerPosition);
    static ::media::Result<MediaGraphPayloadResidence> plan(
        const MediaGraph& graph,
        const MediaGraphPayloadProducerKey& producer,
        const MediaGraphPayloadResidenceFacts& facts);
};

} // namespace media::ffmpeg::graph
